/**
 * 板内 hardcode 时间轴引导（打击或弦乐由编译宏选头文件）
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "song_guide.h"
#include "led_bar.h"

#if defined(DEVICE_ROLE_STR)
#include "song_data_str.h"
#else
#include "song_data_perc.h"
#endif

static const char *TAG = "song_guide";

static volatile int g_playing = 0;
static TickType_t g_t0_tick = 0;
static uint32_t g_delay_ms = 0;
static int g_waiting_delay = 0;
static TaskHandle_t g_task = NULL;

/** keyId 0..8 → 朋友灯带 bit(keyId+1)；定色亮灭 */
static uint16_t mask_from_now(uint32_t now_ms)
{
    uint16_t mask = 0;
    const uint32_t hold = 280;
    for (uint32_t i = 0; i < SONG_EVENT_COUNT; i++) {
        uint32_t t = SONG_EVENTS[i].tMs;
        if (t > now_ms + 40) {
            break;
        }
        if (now_ms + 40 >= t && now_ms <= t + hold) {
            uint8_t kid = SONG_EVENTS[i].keyId;
            if (kid < 9) {
                mask |= (uint16_t)(1u << (kid + 1));
            }
        }
    }
    return mask;
}

static void guide_task(void *arg)
{
    (void)arg;
    uint16_t last = 0xFFFF;
    while (1) {
        if (!g_playing) {
            if (last != 0) {
                led_bar_clear();
                last = 0;
            }
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }
        TickType_t now = xTaskGetTickCount();
        if (g_waiting_delay) {
            if ((now - g_t0_tick) < pdMS_TO_TICKS(g_delay_ms)) {
                vTaskDelay(pdMS_TO_TICKS(10));
                continue;
            }
            g_t0_tick = xTaskGetTickCount();
            g_waiting_delay = 0;
            ESP_LOGI(TAG, "timeline running");
        }
        uint32_t now_ms =
            (uint32_t)((now - g_t0_tick) * portTICK_PERIOD_MS);
        if (now_ms >= SONG_DURATION_MS) {
            ESP_LOGI(TAG, "song done @ %u ms", now_ms);
            g_playing = 0;
            led_bar_clear();
            last = 0;
            continue;
        }
        uint16_t mask = mask_from_now(now_ms);
        if (mask != last) {
            led_bar_show_keys(mask);
            last = mask;
        }
        vTaskDelay(pdMS_TO_TICKS(15));
    }
}

esp_err_t song_guide_init(void)
{
    ESP_LOGI(TAG, "hardcode events=%u duration=%u ms",
             (unsigned)SONG_EVENT_COUNT, (unsigned)SONG_DURATION_MS);
    if (!g_task) {
        xTaskCreate(guide_task, "song_guide", 4096, NULL, 5, &g_task);
    }
    return ESP_OK;
}

void song_guide_start(uint32_t delay_ms)
{
    g_delay_ms = delay_ms;
    g_t0_tick = xTaskGetTickCount();
    g_waiting_delay = delay_ms > 0 ? 1 : 0;
    if (!g_waiting_delay) {
        g_t0_tick = xTaskGetTickCount();
    }
    g_playing = 1;
    ESP_LOGI(TAG, "START delay=%u ms", (unsigned)delay_ms);
}

void song_guide_stop(void)
{
    g_playing = 0;
    g_waiting_delay = 0;
    led_bar_clear();
    ESP_LOGI(TAG, "STOP");
}

int song_guide_is_playing(void)
{
    return g_playing;
}
