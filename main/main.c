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
#include "driver/gpio.h"

#include "oled.h"
#include "nv3007.h"
#include "button.h"
#include "keypad.h"
#include "audio.h"
#include "nvs_flash.h"
#include "ble_app.h"
#include "led_bar.h"
#include "score_data.h"

static const char *TAG = "main";

#define DO_LED_PIN    GPIO_NUM_15
#define RE_LED_PIN    GPIO_NUM_16

#define UI_STYLE      1    /* 0=旧黑底白字(oled)  1=新三栏英文(nv3007) */
#define BLE_ENABLED     0   /* 0=蓝牙常关，1=启动即开 */
#define MODE_COUNT      3
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
    "架子鼓模式", "电子琴模式", "乐谱模式",
};

static const uint32_t NOTE_FREQS[] = { 0, NOTE_DO, NOTE_RE, NOTE_MI, NOTE_FA, NOTE_SOL, NOTE_LA, NOTE_SI, NOTE_DO2, NOTE_RE2 };
static const char *NOTE_NAMES[]     = { "", "Do", "Re", "Mi", "Fa", "Sol", "La", "Si", "HiDo", "HiRe" };
/* 乐谱来自 score_data.h（MIDI 转换生成） */

static uint8_t current_mode = 0;
static uint16_t g_last_mask = 0;   /* 上一次按下的键位掩码（多键） */
static int score_step = 0;
static int score_done = 0;
static int last_score_step = -1;   /* 乐谱模式防闪烁：只在变化时刷新 */
static int seg_playing = 0;        /* 当前段是否在播放 */
static uint32_t seg_start_ms = 0;  /* 段播放开始时间 */
static int seg_note_idx = 0;       /* 段内音符游标 */

void app_main(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  多模式乐器控制器 - 矩阵键盘版");
    ESP_LOGI(TAG, "========================================");

    nvs_flash_init();  /* 蓝牙/WiFi 需要 NVS */
    oled_init();
    button_init();
    keypad_init();
    audio_init();
    led_bar_init();   /* WS2812 灯带 */
#if BLE_ENABLED
    ble_app_init();   /* 蓝牙常关，仅在宏开启时初始化 */
#endif

    gpio_reset_pin(DO_LED_PIN);
    gpio_set_direction(DO_LED_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(DO_LED_PIN, 0);
    gpio_reset_pin(RE_LED_PIN);
    gpio_set_direction(RE_LED_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(RE_LED_PIN, 0);

    ESP_LOGI(TAG, "当前模式: %d", current_mode);
    ESP_LOGI(TAG, "初始化完成");

    while (1) {
        /* 采集所有同时按下的矩阵键，位掩码表示 */
        uint16_t mask = keypad_get_pressed_mask();
        uint16_t new_keys = mask & ~g_last_mask;    /* 本次新按下的键 */
        uint16_t released = g_last_mask & ~mask;    /* 本次松开的键 */
        g_last_mask = mask;

        /* GPIO4 = 模式切换 */
        if (button_read_click()) {
            audio_stop();
            led_bar_clear();
            gpio_set_level(DO_LED_PIN, 0);
            gpio_set_level(RE_LED_PIN, 0);
            current_mode = (current_mode + 1) % MODE_COUNT;
            score_step = 0;
            score_done = 0;
            last_score_step = -1;
            g_last_mask = 0;
            ESP_LOGI(TAG, "→ 模式: %d", current_mode);
            /* 模式切换后立即刷新界面 */
            {
                static const char *mode_en[] = { "DRUM", "PIANO", "SCORE" };
                if (UI_STYLE)
                    draw_ui_three(mode_en[current_mode], "MODE", mode_en[current_mode],
                                  "NOTE", "-", "STATUS", "READY", "GPIO4 SWITCH");
                else
                    oled_draw_center_text(MODE_TEXTS[current_mode]);
            }
        }

        switch (current_mode) {

        case 0: /* 架子鼓：按键播放对应鼓音色 */
            led_bar_clear();
            gpio_set_level(DO_LED_PIN, 0);
            gpio_set_level(RE_LED_PIN, 0);
            for (int k = 1; k <= 9; k++) {
                if (new_keys & (1u << k)) {
                    audio_play_drum(k);
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
            break;

        case 1: /* 电子琴：支持多键同时按下 → 和弦，灯带跟随按键 */
            gpio_set_level(DO_LED_PIN, 0);
            gpio_set_level(RE_LED_PIN, 0);
            led_bar_show_keys(mask);   /* 按下的音键点亮对应灯珠 */
            for (int k = 1; k <= 9; k++) {
                if (new_keys & (1u << k)) {
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
            break;

        case 2: /* 乐谱：只在 score_step 变化时刷新屏幕（防闪烁） */
            if (score_done) {
                led_bar_clear();
                if (last_score_step != -2) {   /* 只画一次 DONE */
                    last_score_step = -2;
                    if (UI_STYLE)
                        draw_ui_three("SCORE", "STATUS", "DONE", "NOTE", "-", "KEY", "-", "SCORE MODE");
                    else
                        oled_draw_center_text("完成");
                }
                audio_stop();
                break;
            }
            {
            int target = SCORE[score_step % SCORE_LEN];
            int dur = SEG_DUR_MS[score_step % SCORE_LEN];   /* 该段时长(ms) */
            uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);
            gpio_set_level(DO_LED_PIN, (target == 1) ? 1 : 0);
            gpio_set_level(RE_LED_PIN, (target == 2) ? 1 : 0);
            led_bar_show_keys(1u << target);   /* 提示该按的段键 → 对应灯珠亮 */

            if (last_score_step != score_step) {   /* 段变化才刷新屏幕 */
                last_score_step = score_step;
                seg_playing = 0;                   /* 等待按键触发 */
                if (UI_STYLE) {
                    char stepbuf[16], kbbuf[8];
                    snprintf(stepbuf, sizeof(stepbuf), "%d/%d", score_step + 1, SCORE_TOTAL);
                    snprintf(kbbuf, sizeof(kbbuf), "%d", target);
                    draw_ui_three("SCORE", "STEP", stepbuf, "NEXT", NOTE_NAMES[target],
                                  "KEY", kbbuf, "PRESS TO PLAY");
                } else {
                    oled_draw_center_text(NOTE_NAMES[target]);
                }
            }

            /* 按对段键 → 开始播放该段所有音符 */
            if (!seg_playing && (new_keys & (1u << target))) {
                seg_playing = 1;
                seg_start_ms = now_ms;
                seg_note_idx = SEG_NOTE_START[score_step];
                audio_play_freq(NOTE_FREQS[target]);   /* 先播代表键 */
                ESP_LOGI(TAG, "乐谱段 %d/%d: 开始播放", score_step + 1, SCORE_TOTAL);
            }

            /* 段播放中：按时间触发段内音符 */
            if (seg_playing) {
                int end_idx = SEG_NOTE_START[score_step] + SEG_NOTE_COUNT[score_step];
                while (seg_note_idx < end_idx) {
                    int off = SEG_NOTES_OFFSET[seg_note_idx];
                    if (now_ms - seg_start_ms >= (uint32_t)off) {
                        audio_play_freq(NOTE_FREQS[SEG_NOTES_KEY[seg_note_idx]]);
                        seg_note_idx++;
                    } else break;
                }
                /* 段时长到 → 进下一段 */
                if (now_ms - seg_start_ms >= (uint32_t)dur) {
                    score_step++;
                }
            }

            if (score_step >= SCORE_TOTAL) {
                score_done = 1;
                gpio_set_level(DO_LED_PIN, 0);
                gpio_set_level(RE_LED_PIN, 0);
            }
            /* 松开的键停音 */
            for (int k = 1; k <= 9; k++)
                if (released & (1u << k))
                    audio_stop_freq(NOTE_FREQS[k]);
            }
            break;

        }
        /* 音量调节（电位器 ADC，每 500ms 更新，不再显示在屏幕） */
        {
            static int vol_tick = 0;
            if (++vol_tick >= 50) {
                vol_tick = 0;
                audio_update_volume();
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
