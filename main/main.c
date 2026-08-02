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
#include "driver/gpio.h"

#include "oled.h"
#include "button.h"
#include "keypad.h"
#include "touch.h"
#include "audio.h"
#include "nvs_flash.h"
#include "ble_app.h"
#include "led_bar.h"

static const char *TAG = "main";

#define DO_LED_PIN    GPIO_NUM_15
#define RE_LED_PIN    GPIO_NUM_16

#define BLE_ENABLED     0   /* 0=蓝牙常关，1=启动即开 */
#define MODE_COUNT      4
#define SCORE_LOOPS      2
#define SCORE_TOTAL      (SCORE_LEN * SCORE_LOOPS)

static const char *MODE_TEXTS[] = {
    "架子鼓模式", "电子琴模式", "乐谱模式", "蓝牙模式",
};

static const uint32_t NOTE_FREQS[] = { 0, NOTE_DO, NOTE_RE, NOTE_MI, NOTE_FA, NOTE_SOL, NOTE_LA, NOTE_SI, NOTE_DO2, NOTE_RE2, NOTE_MI2, NOTE_HIFA, NOTE_HISOL, NOTE_HILA };
static const char *NOTE_NAMES[]     = { "", "Do", "Re", "Mi", "Fa", "Sol", "La", "Si", "HiDo", "HiRe", "HiMi", "HiFa", "HiSol", "HiLa" };
static const int SCORE[] = {1,1,5,5,6,6,5}; 
#define SCORE_LEN        7

static uint8_t current_mode = 0;
static uint16_t g_last_mask = 0;   /* 上一次按下的键位掩码（多键） */
static int score_step = 0;
static int score_done = 0;

void app_main(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  多模式乐器控制器 - 矩阵键盘版");
    ESP_LOGI(TAG, "========================================");

    nvs_flash_init();  /* 蓝牙/WiFi 需要 NVS */
    oled_init();
    button_init();
    keypad_init();
    touch_init();
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

    oled_draw_center_text(MODE_TEXTS[current_mode]);
    ESP_LOGI(TAG, "当前模式: %d", current_mode);
    ESP_LOGI(TAG, "初始化完成");

    while (1) {
        /* 采集所有同时按下的键（矩阵 + 触摸），位掩码表示 */
        uint16_t mask = keypad_get_pressed_mask();
        int tk = touch_get_key();
        if (tk) mask |= (1u << tk);
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
            g_last_mask = 0;
            oled_draw_center_text(MODE_TEXTS[current_mode]);
            ESP_LOGI(TAG, "→ 模式: %d", current_mode);
        }

        switch (current_mode) {

        case 0: /* 架子鼓 */
            audio_stop();
            led_bar_clear();
            gpio_set_level(DO_LED_PIN, 0);
            gpio_set_level(RE_LED_PIN, 0);
            break;

        case 1: /* 电子琴：支持多键同时按下 → 和弦，灯带跟随按键 */
            gpio_set_level(DO_LED_PIN, 0);
            gpio_set_level(RE_LED_PIN, 0);
            led_bar_show_keys(mask);   /* 按下的音键点亮对应灯珠 */
            for (int k = 1; k <= 13; k++) {
                if (new_keys & (1u << k)) {
                    audio_play_freq(NOTE_FREQS[k]);
                    oled_draw_center_text(NOTE_NAMES[k]);   /* 显示最后按下的音名 */
                    ESP_LOGI(TAG, "弹奏: %s (key=%d)", NOTE_NAMES[k], k);
                }
                if (released & (1u << k))
                    audio_stop_freq(NOTE_FREQS[k]);
            }
            break;

        case 2: /* 乐谱 */
            if (score_done) {
                led_bar_clear();
                oled_draw_center_text("完成");
                audio_stop();
                break;
            }
            {
            int target = SCORE[score_step % SCORE_LEN];
            gpio_set_level(DO_LED_PIN, (target == 1) ? 1 : 0);
            gpio_set_level(RE_LED_PIN, (target == 2) ? 1 : 0);
            oled_draw_center_text(NOTE_NAMES[target]);
            led_bar_show_keys(1u << target);   /* 提示该按的键 → 对应灯珠亮 */

            if ((new_keys & (1u << target))) {
                audio_play_freq(NOTE_FREQS[target]);
                score_step++;
                ESP_LOGI(TAG, "乐谱 %d/%d: %s", score_step, SCORE_TOTAL, NOTE_NAMES[target]);
                if (score_step >= SCORE_TOTAL) {
                    score_done = 1;
                    gpio_set_level(DO_LED_PIN, 0);
                    gpio_set_level(RE_LED_PIN, 0);
                    oled_draw_center_text("完成");
                }
            }
            /* 松开的键停音 */
            for (int k = 1; k <= 13; k++)
                if (released & (1u << k))
                    audio_stop_freq(NOTE_FREQS[k]);
            }
            break;

        case 3: { /* 蓝牙 */
            gpio_set_level(DO_LED_PIN, 0);
            gpio_set_level(RE_LED_PIN, 0);
            audio_stop();
            led_bar_clear();
#if BLE_ENABLED
            if (ble_app_is_connected())
                oled_draw_two_lines("蓝牙模式", "已连接");
            else
                oled_draw_two_lines("蓝牙模式", "等待中...");
#else
            oled_draw_two_lines("蓝牙模式", "等待中...");
#endif
            break;
        }

        }
        /* 音量 ADC + 右上角显示（每 500ms，局部刷新不占带宽） */
        {
            static int vol_tick = 0;
            if (++vol_tick >= 50) {
                vol_tick = 0;
                audio_update_volume();
                oled_draw_vol(audio_get_volume());
                touch_print_raw();   /* 调试：打印触摸电压变化 */
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
