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

static const char *TAG = "main";

#define DO_LED_PIN    GPIO_NUM_15
#define RE_LED_PIN    GPIO_NUM_16

#define MODE_COUNT      5
#define SCORE_LOOPS      2
#define SCORE_TOTAL      (SCORE_LEN * SCORE_LOOPS)

static const char *MODE_TEXTS[] = {
    "架子鼓模式", "吉他模式", "电子琴模式", "乐谱模式", "蓝牙模式",
};

static const uint32_t NOTE_FREQS[] = { 0, NOTE_DO, NOTE_RE, NOTE_MI, NOTE_FA, NOTE_SOL, NOTE_LA, NOTE_SI, NOTE_DO2, NOTE_RE2, NOTE_MI2 };
static const char *NOTE_NAMES[]     = { "", "Do", "Re", "Mi", "Fa", "Sol", "La", "Si", "HiDo", "HiRe", "HiMi" };
static const int SCORE[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
#define SCORE_LEN        10

static uint8_t current_mode = 0;
static int last_key = 0;
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
    ble_app_init();   /* 启动时初始化 BLE，所有模式下保持连接 */

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
        int key = keypad_get_key();
        if (key == 0) key = touch_get_key();  /* 矩阵按键优先，其次触摸 */
        int key_pressed = (key != 0 && key != last_key);
        last_key = key;

        /* GPIO4 = 模式切换 */
        if (button_read_click()) {
            audio_stop();
            gpio_set_level(DO_LED_PIN, 0);
            gpio_set_level(RE_LED_PIN, 0);
            current_mode = (current_mode + 1) % MODE_COUNT;
            score_step = 0;
            score_done = 0;
            oled_draw_center_text(MODE_TEXTS[current_mode]);
            ESP_LOGI(TAG, "→ 模式: %d", current_mode);
        }

        switch (current_mode) {

        case 0: /* 架子鼓 */
        case 1: /* 吉他 */
            audio_stop();
            gpio_set_level(DO_LED_PIN, 0);
            gpio_set_level(RE_LED_PIN, 0);
            break;

        case 2: /* 电子琴 */
            gpio_set_level(DO_LED_PIN, 0);
            gpio_set_level(RE_LED_PIN, 0);
            if (key_pressed) {
                audio_play_freq(NOTE_FREQS[key]);
                oled_draw_center_text(NOTE_NAMES[key]);
                ESP_LOGI(TAG, "弹奏: %s", NOTE_NAMES[key]);
            } else if (key == 0) {
                audio_stop();
                oled_draw_center_text(MODE_TEXTS[current_mode]);
            }
            break;

        case 3: /* 乐谱 */
            if (score_done) {
                oled_draw_center_text("完成");
                audio_stop();
                break;
            }
            {
            int target = SCORE[score_step % SCORE_LEN];
            gpio_set_level(DO_LED_PIN, (target == 1) ? 1 : 0);
            gpio_set_level(RE_LED_PIN, (target == 2) ? 1 : 0);
            oled_draw_center_text(NOTE_NAMES[target]);

            if (key_pressed && key == target) {
                audio_play_freq(NOTE_FREQS[key]);
                score_step++;
                ESP_LOGI(TAG, "乐谱 %d/%d: %s", score_step, SCORE_TOTAL, NOTE_NAMES[key]);
                if (score_step >= SCORE_TOTAL) {
                    score_done = 1;
                    gpio_set_level(DO_LED_PIN, 0);
                    gpio_set_level(RE_LED_PIN, 0);
                    oled_draw_center_text("完成");
                }
            }
            if (key == 0) audio_stop();
            }
            break;

        case 4: { /* 蓝牙 */
            gpio_set_level(DO_LED_PIN, 0);
            gpio_set_level(RE_LED_PIN, 0);
            audio_stop();
            if (ble_app_is_connected())
                oled_draw_two_lines("蓝牙模式", "已连接");
            else
                oled_draw_two_lines("蓝牙模式", "等待中...");
            break;
        }

        }
        /* 音量 ADC + 右上角显示（每 500ms 刷新一次屏幕，
         * 避免每 10ms 全屏刷新 40KB 抢占 SPI 带宽影响音频） */
        {
            static int vol_tick = 0;
            if (++vol_tick >= 50) {
                vol_tick = 0;
                audio_update_volume();
                oled_draw_vol(audio_get_volume());
                oled_refresh();
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
