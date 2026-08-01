/**
 * ==============================================================
 * 按键组件实现 — GPIO 按键扫描 + 软件消抖
 * ==============================================================
 * 使用简单的状态机来跟踪按键的按下和释放过程
 *
 * 状态转换图：
 *   IDLE ──(检测到低电平)──> DEBOUNCE_DOWN ──(确认低)──> PRESSED
 *   PRESSED ──(检测到高电平)──> DEBOUNCE_UP ──(确认高)──> IDLE (返回 1)
 * ==============================================================
 */

#include "button.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "button";

/* --- 按键状态机枚举 --- */
typedef enum {
    BTN_STATE_IDLE,          // 空闲：等待按下
    BTN_STATE_DEBOUNCE_DOWN, // 正在消抖（判断是否真的按下）
    BTN_STATE_PRESSED,       // 已按下：等待释放
    BTN_STATE_DEBOUNCE_UP,   // 正在消抖（判断是否真的释放）
} btn_state_t;

/* --- 状态机相关全局变量 --- */
static btn_state_t g_btn_state = BTN_STATE_IDLE;  // 当前状态
static uint32_t g_debounce_time = 0;              // 消抖计时起点（ms）

/**
 * @brief 获取当前系统运行时间（毫秒）
 *        使用 FreeRTOS 的 xTaskGetTickCount 乘以 tick 周期
 */
static uint32_t get_ms(void) {
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

esp_err_t button_init(void) {
    ESP_LOGI(TAG, "正在初始化按键 GPIO...");

    // GPIO 配置结构体（ESP-IDF 标准写法）
    gpio_config_t gpio_conf = {
        .pin_bit_mask = (1ULL << BTN_GPIO),         // 选择引脚
        .mode = GPIO_MODE_INPUT,                     // 输入模式
        .pull_up_en = GPIO_PULLUP_ENABLE,            // 启用内部上拉（默认高电平）
        .pull_down_en = GPIO_PULLDOWN_DISABLE,       // 禁用下拉
        .intr_type = GPIO_INTR_DISABLE,              // 不使用中断（轮询模式）
    };
    gpio_config(&gpio_conf);

    ESP_LOGI(TAG, "按键初始化完成 (GPIO %d)", BTN_GPIO);
    return ESP_OK;
}

uint8_t button_is_pressed(void) {
    // 低电平 = 按键被按下（因为上拉，按下时接地）
    return (gpio_get_level(BTN_GPIO) == 0) ? 1 : 0;
}

uint8_t button_read_click(void) {
    uint8_t level = gpio_get_level(BTN_GPIO);  // 读取当前引脚电平
    uint32_t now = get_ms();                    // 获取当前时间

    switch (g_btn_state) {

    // --- 状态 0：空闲，等待按键按下 ---
    case BTN_STATE_IDLE:
        if (level == 0) {                         // 检测到低电平（可能按下了）
            g_btn_state = BTN_STATE_DEBOUNCE_DOWN; // 进入消抖状态
            g_debounce_time = now;                 // 记录消抖开始时间
        }
        break;

    // --- 状态 1：按下消抖，等待 20ms 后再次确认 ---
    case BTN_STATE_DEBOUNCE_DOWN:
        if ((now - g_debounce_time) >= BTN_DEBOUNCE_MS) {
            // 消抖时间到，再次读取引脚
            if (gpio_get_level(BTN_GPIO) == 0) {
                // 确认是真实按下（不是干扰脉冲）
                g_btn_state = BTN_STATE_PRESSED;
                ESP_LOGD(TAG, "按键按下确认");
            } else {
                // 是干扰，回到空闲
                g_btn_state = BTN_STATE_IDLE;
            }
        }
        break;

    // --- 状态 2：按键已按下，等待释放 ---
    case BTN_STATE_PRESSED:
        if (level == 1) {                         // 检测到高电平（可能松开了）
            g_btn_state = BTN_STATE_DEBOUNCE_UP;   // 进入释放消抖状态
            g_debounce_time = now;
        }
        break;

    // --- 状态 3：释放消抖，等待 20ms 后再次确认 ---
    case BTN_STATE_DEBOUNCE_UP:
        if ((now - g_debounce_time) >= BTN_DEBOUNCE_MS) {
            if (gpio_get_level(BTN_GPIO) == 1) {
                // 确认释放，一次完整点击完成！
                ESP_LOGD(TAG, "按键释放确认 → 一次完整点击");
                g_btn_state = BTN_STATE_IDLE;
                return 1;  // ✅ 返回点击事件
            } else {
                // 仍未释放，回到按下状态
                g_btn_state = BTN_STATE_PRESSED;
            }
        }
        break;
    }

    return 0;  // 没有点击事件
}
