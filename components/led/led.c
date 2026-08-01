/**
 * ==============================================================
 * LED 指示灯组件实现
 * ==============================================================
 */

#include "driver/gpio.h"
#include "esp_log.h"
#include "led.h"

static const char *TAG = "led";

/* GPIO 映射 */
static const int LED_PINS[] = {
    0,          /* 0 = 无效 */
    GPIO_NUM_20, /* LED_DO = 1 */
    GPIO_NUM_21, /* LED_RE = 2 */
    GPIO_NUM_47, /* LED_MI = 3 */
    GPIO_NUM_48, /* LED_FA = 4 */
};

esp_err_t led_init(void)
{
    ESP_LOGI(TAG, "正在初始化 LED 指示灯...");

    for (int i = 1; i <= 4; i++) {
        gpio_reset_pin(LED_PINS[i]);
        gpio_set_direction(LED_PINS[i], GPIO_MODE_OUTPUT);
        gpio_set_level(LED_PINS[i], 0);
        ESP_LOGI(TAG, "  LED %d → GPIO %d", i, (int)LED_PINS[i]);
    }

    ESP_LOGI(TAG, "LED 初始化完成");
    return ESP_OK;
}

void led_on(int note)
{
    if (note >= 1 && note <= 4) {
        gpio_set_level(LED_PINS[note], 1);
    }
}

void led_off(int note)
{
    if (note >= 1 && note <= 4) {
        gpio_set_level(LED_PINS[note], 0);
    }
}

void led_all_off(void)
{
    for (int i = 1; i <= 4; i++) {
        gpio_set_level(LED_PINS[i], 0);
    }
}
