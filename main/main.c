/**
 * ==============================================================
 * ESP32-S3 多模式乐器控制器 — 矩阵键盘版
 * ==============================================================
 * GPIO4 = 模式切换键（始终有效）
 *
 * 模式 0: 架子鼓  1: 吉他  2: 电子琴  3: 乐谱  4: 蓝牙
 *
 * 矩阵键盘: 3行(GPIO38-39-21)×3列(GPIO35-37)=9键(Do~高Re)
 * 触摸: GPIO14 = 高Mi
 * ==============================================================
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "driver/gpio.h"

#include "oled.h"
#include "nv3007.h"
#include "button.h"
#include "keypad.h"
#include "audio.h"
#include "nvs_flash.h"
#include "ble_app.h"
#include "led_bar.h"
#include "song_guide.h"

static const char *TAG = "main";


#define UI_STYLE      1    /* 0=旧黑底白字(oled)  1=新三栏英文(nv3007) */
#define BLE_ENABLED     1   /* 1=启动即开（蓝牙广播 Embryo-Perc） */
#define MODE_COUNT      4
#define SCORE_LOOPS      1   /* MIDI 整首歌一遍 */
#define SCORE_TOTAL      (SCORE_LEN * SCORE_LOOPS)

/* ---- 三栏 UI 颜色（蓝绿配色，背景黑） ---- */
#define C_BLACK   0x0000
#define C_WHITE   0xFFFF
#define C_CYAN    0x07FF
#define C_BLUE    0x001F   /* 纯蓝（顶条） */
#define C_GREEN   0x07E0
#define C_DGREEN  0x03E0   /* 深绿（边框/底条） */
#define C_RED     0xF800
#define C_YELLOW  0xFFE0
#define C_DARK    0x18C3

/* ---- 三栏布局（横屏 428x142，照抄 1.txt drawInterface） ---- */
static void draw_ui_three(const char *top, const char *ll, const char *lv,
                          const char *ml, const char *mv, const char *rl, const char *rv,
                          const char *bottom)
{
    nv3007_ui_clear();
    nv3007_ui_fill_rect(0, 0, OLED_WIDTH, 24, C_BLUE);          /* 顶条（蓝） */
    nv3007_ui_panel_text(top, 0, 300, 5, 2, C_WHITE);
    /* 左上角音量显示（VOL0-100） */
    {
        char vbuf[16];
        snprintf(vbuf, sizeof(vbuf), "VOL%u", (unsigned)audio_get_volume());
        nv3007_ui_text(4, 7, vbuf, 1, C_WHITE);
    }
    /* 右上角显示蓝牙状态（不占用模式） */
#if BLE_ENABLED
    nv3007_ui_panel_text(ble_app_is_connected() ? "CONN" : "WAIT", 320, OLED_WIDTH, 8, 1, C_GREEN);
#else
    nv3007_ui_panel_text("BLE OFF", 330, OLED_WIDTH, 8, 1, C_GREEN);
#endif
    nv3007_ui_draw_vline(140, 28, 90, C_DGREEN);                /* 竖线（深绿） */
    nv3007_ui_draw_vline(290, 28, 90, C_DGREEN);
    nv3007_ui_panel_text(ll, 0, 140, 36, 1, C_CYAN);            /* 左栏标签（青） */
    nv3007_ui_panel_text(lv, 0, 140, 60, 3, C_WHITE);
    nv3007_ui_panel_text(ml, 140, 290, 36, 1, C_CYAN);          /* 中栏标签（青） */
    nv3007_ui_panel_text(mv, 140, 290, 60, 3, C_GREEN);         /* 中栏值（绿） */
    nv3007_ui_panel_text(rl, 290, OLED_WIDTH, 36, 1, C_CYAN);   /* 右栏标签（青） */
    nv3007_ui_panel_text(rv, 290, OLED_WIDTH, 60, 2, C_WHITE);
    nv3007_ui_fill_rect(0, 124, OLED_WIDTH, 18, C_DGREEN);      /* 底条（深绿） */
    nv3007_ui_panel_text(bottom, 0, OLED_WIDTH, 130, 1, C_WHITE);
    nv3007_ui_refresh();
}

static const char *MODE_TEXTS[] = {
    "架子鼓模式", "电子琴模式", "乐谱模式", "蓝牙模式",
};

static const uint32_t NOTE_FREQS[] = { 0, NOTE_DO, NOTE_RE, NOTE_MI, NOTE_FA, NOTE_SOL, NOTE_LA, NOTE_SI, NOTE_DO2, NOTE_RE2 };
static const char *NOTE_NAMES[]     = { "", "Do", "Re", "Mi", "Fa", "Sol", "La", "Si", "HiDo", "HiRe" };
/* 乐谱来自 score_data.h（MIDI 转换生成） */

static uint8_t current_mode = 0;
static uint16_t g_last_mask = 0;   /* 上一次按下的键位掩码（多键） */
static uint8_t ble_instrument = 0; /* BLE 播放乐器：0=架子鼓 1=钢琴（双击切换） */
static int score_step = 0;
static int score_done = 0;
static int last_score_step = -1;   /* 乐谱模式防闪烁：只在变化时刷新 */
static int seg_playing = 0;        /* 当前段是否在播放 */
static uint32_t seg_start_ms = 0;  /* 段播放开始时间 */
static int seg_note_idx = 0;       /* 段内音符游标 */
static int last_ble_playing = -1;  /* BLE 模式状态防闪烁 */

void app_main(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  多模式乐器控制器 - 矩阵键盘版");
    ESP_LOGI(TAG, "========================================");

    nvs_flash_init();  /* 蓝牙/WiFi 需要 NVS */
    nv3007_init();     /* 只初始化屏幕（UI_STYLE 1 用 nv3007_ui，省 RAM 给 BLE） */
    button_init();
    keypad_init();
    audio_init();
    led_bar_init();   /* WS2812 灯带 */
    song_guide_init();   /* 时间轴引导任务 */
    song_guide_set_song(0);   /* 默认青修(0)；乐谱模式切到 Believer(1) */
#if BLE_ENABLED
    ble_app_init();   /* BLE 广播 Embryo-Perc */
#endif


    ESP_LOGI(TAG, "当前模式: %d", current_mode);
    ESP_LOGI(TAG, "初始化完成");

    while (1) {
        /* 采集所有同时按下的矩阵键，位掩码表示 */
        uint16_t mask = keypad_get_pressed_mask();
        uint16_t new_keys = mask & ~g_last_mask;    /* 本次新按下的键 */
        uint16_t released = g_last_mask & ~mask;    /* 本次松开的键 */
        g_last_mask = mask;

        /* 调试：按键扫描状态（确认按键是否被检测到） */
        if (new_keys || released)
            ESP_LOGI(TAG, "[按键] mask=0x%04X new=0x%04X rel=0x%04X row=%d col=%d",
                     mask, new_keys, released, keypad_get_row(), keypad_get_col());

        /* GPIO4 = 单击切模式；双击（BLE 已连接）切乐器 */
        uint8_t bevt = button_read_event();
        if (bevt == 1) {   /* 单击 → 模式切换 */
            audio_stop();
            led_bar_clear();
            current_mode = (current_mode + 1) % MODE_COUNT;
            score_step = 0;
            score_done = 0;
            last_score_step = -1;
            g_last_mask = 0;
            /* 只用青修(0)；Believer(1) 数据保留在注册表，供后续烧录时切换 */
            song_guide_set_song(0);
            ESP_LOGI(TAG, "→ 模式: %d", current_mode);
            /* 模式切换后立即刷新界面 */
            {
                static const char *mode_en[] = { "DRUM", "PIANO", "SCORE", "BLE" };
                if (UI_STYLE)
                    draw_ui_three(mode_en[current_mode], "MODE", mode_en[current_mode],
                                  "NOTE", "-", "STATUS", "READY", "GPIO4 SWITCH");
                else
                    oled_draw_center_text(MODE_TEXTS[current_mode]);
            }
        } else if (bevt == 2) {   /* 双击 → 仅 BLE 已连接时切乐器 */
#if BLE_ENABLED
            if (current_mode == 3 && ble_app_is_connected()) {
                ble_instrument = !ble_instrument;
                song_guide_set_instrument(ble_instrument);
                audio_stop();   /* 清残留音 */
                ESP_LOGI(TAG, "BLE 乐器 → %s", ble_instrument ? "钢琴" : "架子鼓");
                if (UI_STYLE)
                    draw_ui_three("BLE", "MODE", "BLE",
                                  "INST", ble_instrument ? "PIANO" : "DRUM",
                                  "STATUS", song_guide_is_playing() ? "PLAY" : "WAIT",
                                  "WEB CONTROL");
                else
                    oled_draw_center_text(MODE_TEXTS[3]);
            }
#endif
        }

        /* 仅架子鼓/电子琴模式停 Believer 时间轴（乐谱/BLE 可播 Believer） */
        if (current_mode != 2 && current_mode != 3 && song_guide_is_playing())
            song_guide_stop();

        /* 调试：长按切换模式键 3 秒 → 整机重启 */
        {
            static int64_t hold_start = -1;
            if (button_is_pressed()) {
                if (hold_start < 0) hold_start = esp_timer_get_time();
                else if ((esp_timer_get_time() - hold_start) >= 3000 * 1000LL) {
                    ESP_LOGI(TAG, "长按 3 秒 → 重启设备");
                    esp_restart();
                }
            } else {
                hold_start = -1;
            }
        }

        switch (current_mode) {

        case 0: /* 架子鼓：按键播放对应鼓音色 + 随机色亮灯 */
            for (int k = 1; k <= 9; k++) {
                if (new_keys & (1u << k)) {
                    audio_play_drum(k);
                    led_bar_randomize_key(k);   /* 每次按下分配随机色 */
                    if (UI_STYLE) {
                        /* 类别：1-4鼓(D) 5-8镲(C) 9低鼓(BD) */
                        const char *cat = (k <= 4) ? "D" : (k <= 8) ? "C" : "BD";
                        char kb[8];
                        snprintf(kb, sizeof(kb), "%d", k);
                        draw_ui_three("DRUM", "TYPE", cat, "KEY", kb, "STATUS", "HIT", "9-KEY DRUM");
                    } else {
                        oled_draw_center_text(MODE_TEXTS[0]);
                    }
                }
            }
            led_bar_show_keys_random(mask);   /* 亮当前按下的键（随机色），松键即灭 */
            /* 松开任何键 → 停鼓音（鼓 voice freq=0） */
            if (released)
                audio_stop_freq(0);
            break;

        case 1: /* 电子琴：支持多键同时按下 → 和弦，灯带跟随按键（随机色） */
            for (int k = 1; k <= 9; k++) {
                if (new_keys & (1u << k)) {
                    led_bar_randomize_key(k);   /* 每次按下分配随机色 */
                    audio_play_freq(NOTE_FREQS[k]);
                    /* 显示最后按下的音名（两套 UI） */
                    if (UI_STYLE) {
                        char kb[8];
                        snprintf(kb, sizeof(kb), "%d", k);
                        draw_ui_three("PIANO", "NOTE", NOTE_NAMES[k], "KEY", kb,
                                      "STATUS", "PLAY", "9-KEY PIANO");
                    } else {
                        oled_draw_center_text(NOTE_NAMES[k]);
                    }
                    ESP_LOGI(TAG, "弹奏: %s (key=%d)", NOTE_NAMES[k], k);
                }
                if (released & (1u << k))
                    audio_stop_freq(NOTE_FREQS[k]);
            }
            led_bar_show_keys_random(mask);   /* 亮当前按下的键（随机色），松键即灭 */
            break;

        case 2: /* 一键播歌：按任意键播放整首青春修炼手册（GM鼓+灯带） */
            {
                int playing = song_guide_is_playing();
                /* 按任意键 → 启动青修整首播放 */
                if (new_keys && !playing) {
                    audio_stop();
                    song_guide_start(0);
                    ESP_LOGI(TAG, "一键播歌: 开始播放青修");
                }
                /* 状态变化才刷新屏幕（防闪烁） */
                if (last_score_step != (playing ? 1 : 0)) {
                    last_score_step = playing ? 1 : 0;
                    if (UI_STYLE)
                        draw_ui_three("SCORE", "MODE", "QINGCHUN",
                                      "STATUS", playing ? "PLAY" : "PRESS",
                                      "-", "-", "PRESS KEY TO PLAY");
                    else
                        oled_draw_center_text(MODE_TEXTS[2]);
                }
            }
            break;

        case 3: /* 蓝牙模式：时间轴亮灯 + 按键发声（START 后才响，按乐器模式发一种） */
            /* 灯带交给 song_guide（START 后按曲目节奏） */
            {
                int playing = song_guide_is_playing();
                if (last_ble_playing != playing) {   /* 状态变化才刷新屏幕 */
                    last_ble_playing = playing;
                    if (UI_STYLE)
                        draw_ui_three("BLE", "MODE", "BLE",
                                      "STATUS", playing ? "PLAY" : "WAIT",
                                      "INST", ble_instrument ? "PIANO" : "DRUM",
                                      "WEB CONTROL");
                    else
                        oled_draw_center_text(MODE_TEXTS[3]);
                }
            }
            /* 按键：仅 START 后发声；只发一种乐器；松开即停（不影响时间轴自动播歌） */
            for (int k = 1; k <= 9; k++) {
                if (new_keys & (1u << k)) {
                    if (song_guide_is_playing()) {
                        if (ble_instrument == 1)
                            audio_play_freq(NOTE_FREQS[k]);   /* 钢琴 */
                        else
                            audio_play_drum_key(k);           /* 架子鼓(标记频率) */
                        ESP_LOGI(TAG, "BLE键 %d: %s", k, ble_instrument ? "钢琴" : "鼓");
                    }
                }
                if (released & (1u << k)) {
                    if (ble_instrument == 1)
                        audio_stop_freq(NOTE_FREQS[k]);       /* 停钢琴 */
                    else
                        audio_stop_drum_key(k);               /* 停手动鼓 */
                }
            }
            break;

        }
        /* 手动演奏模式（鼓0/琴1）无键按下 → 强制静音，杜绝残留音。
         * 乐谱(2)/蓝牙(3) 的自主时间轴播放不能被"无键静音"打断（否则整曲被掐静音）。 */
        if (mask == 0 && current_mode <= 1) {
            audio_stop();
            seg_playing = 0;
        }
        /* 音量调节（电位器 ADC，每 500ms 更新；变化时按屏幕刷新节奏重绘左上角音量） */
        {
            static int vol_tick = 0;
            static uint8_t last_vol = 255;
            if (++vol_tick >= 50) {
                vol_tick = 0;
                audio_update_volume();
                uint8_t v = audio_get_volume();
                if (v != last_vol) {
                    last_vol = v;
                    if (UI_STYLE) {
                        char vbuf[16];
                        snprintf(vbuf, sizeof(vbuf), "VOL%u", (unsigned)v);
                        nv3007_ui_text(4, 7, vbuf, 1, C_WHITE);
                        nv3007_ui_refresh();
                    }
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
