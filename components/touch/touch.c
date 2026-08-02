/**
 * ==============================================================
 * Touch 电容触摸 — 1 通道 (TOUCH14 → GPIO14 = 高Mi)
 * ==============================================================
 */

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/touch_sens.h"
#include "touch.h"

static const char *TAG = "touch";

/* 通道映射: TOUCH14 → 返回 key 10 */
static const int CHANNELS[TOUCH_NUM] = { 14 };
static const int KEY_MAP[TOUCH_NUM]  = { 10 };  /* 高Mi */

static touch_sensor_handle_t g_sens = NULL;
static touch_channel_handle_t g_chan[TOUCH_NUM];
static uint32_t g_baseline[TOUCH_NUM];
static int g_calibrated = 0;

esp_err_t touch_init(void)
{
    ESP_LOGI(TAG, "初始化 1 键触摸 (GPIO14=高Mi)...");

    touch_sensor_sample_config_t s_cfg =
        TOUCH_SENSOR_V2_DEFAULT_SAMPLE_CONFIG(500, TOUCH_VOLT_LIM_L_0V5, TOUCH_VOLT_LIM_H_2V7);
    touch_sensor_config_t sens_cfg = TOUCH_SENSOR_DEFAULT_BASIC_CONFIG(1, &s_cfg);
    sens_cfg.power_on_wait_us = 512;
    ESP_ERROR_CHECK(touch_sensor_new_controller(&sens_cfg, &g_sens));

    for (int i = 0; i < TOUCH_NUM; i++) {
        touch_channel_config_t ch_cfg = {
            .active_thresh = { 5000 },
            .charge_speed = TOUCH_CHARGE_SPEED_7,
            .init_charge_volt = TOUCH_INIT_CHARGE_VOLT_LOW,
        };
        ESP_ERROR_CHECK(touch_sensor_new_channel(g_sens, CHANNELS[i], &ch_cfg, &g_chan[i]));
        ESP_LOGI(TAG, "  TOUCH%d (GPIO%d) → key %d", CHANNELS[i], CHANNELS[i], KEY_MAP[i]);
    }

    touch_sensor_filter_config_t f_cfg = TOUCH_SENSOR_DEFAULT_FILTER_CONFIG();
    ESP_ERROR_CHECK(touch_sensor_config_filter(g_sens, &f_cfg));

    ESP_ERROR_CHECK(touch_sensor_enable(g_sens));
    ESP_ERROR_CHECK(touch_sensor_start_continuous_scanning(g_sens));

    vTaskDelay(pdMS_TO_TICKS(300));
    for (int i = 0; i < TOUCH_NUM; i++)
        touch_channel_read_data(g_chan[i], TOUCH_CHAN_DATA_TYPE_SMOOTH, &g_baseline[i]);

    g_calibrated = 1;
    ESP_LOGI(TAG, "触摸初始化完成");
    return ESP_OK;
}

/* 触摸防误触：阈值 + 连续确认 */
#define TOUCH_THRESH     4        /* val > baseline*4 才判定为按下 */
#define TOUCH_CONFIRM    3        /* 连续 3 次超阈值才真正触发 */

static int g_confirm = 0;
static int g_touch_state = 0;

int touch_get_key(void)
{
    if (!g_calibrated) return 0;

    uint32_t val;
    if (touch_channel_read_data(g_chan[0], TOUCH_CHAN_DATA_TYPE_SMOOTH, &val) != ESP_OK)
        return 0;

    if (val > g_baseline[0] * TOUCH_THRESH) {
        /* 连续确认：防偶发误触（如手靠近/环境干扰） */
        if (g_confirm < TOUCH_CONFIRM) {
            g_confirm++;
        } else {
            g_touch_state = 1;
        }
    } else {
        g_confirm = 0;
        g_touch_state = 0;
    }

    /* 未触摸时缓慢更新基线 */
    if (val < g_baseline[0] * 2)
        g_baseline[0] = (g_baseline[0] * 99 + val) / 100;
    else if (val < g_baseline[0] / 2)
        g_baseline[0] = val;  /* 基线被污染，立即复位 */

    return g_touch_state ? KEY_MAP[0] : 0;
}
