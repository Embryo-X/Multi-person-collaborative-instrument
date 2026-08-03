/**
 * ==============================================================
 * LED 灯带 — WS2812 可寻址灯珠 (GPIO20)
 * RMT 驱动，9 灯珠对应矩阵键盘 9 音
 * ==============================================================
 * WS2812 时序 (10MHz RMT, 100ns/tick):
 *   0 码: 高 400ns + 低 850ns
 *   1 码: 高 850ns + 低 400ns
 *   数据序: GRB, MSB first
 * ==============================================================
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_check.h"
#include "esp_log.h"
#include "driver/rmt_tx.h"
#include "driver/rmt_encoder.h"
#include "driver/rmt_types.h"

#include "led_bar.h"

static const char *TAG = "led_bar";

#define LED_BAR_PIN      GPIO_NUM_20
#define LED_BAR_RES_HZ   (10 * 1000 * 1000)   /* 100ns/tick */
#define BITS_PER_LED     24

static rmt_channel_handle_t g_chan = NULL;
static rmt_encoder_handle_t g_enc = NULL;

/* WS2812 颜色 (GRB 顺序) */
typedef struct { uint8_t g, r, b; } grb_t;
static grb_t g_colors[LED_BAR_COUNT];

esp_err_t led_bar_init(void)
{
    rmt_tx_channel_config_t tx_cfg = {
        .gpio_num = LED_BAR_PIN,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = LED_BAR_RES_HZ,
        .mem_block_symbols = 256,           /* 9*24=216 symbols 够用 */
        .trans_queue_depth = 4,
    };
    ESP_RETURN_ON_ERROR(rmt_new_tx_channel(&tx_cfg, &g_chan), TAG, "rmt_new_tx_channel");

    rmt_copy_encoder_config_t enc_cfg = {};
    ESP_RETURN_ON_ERROR(rmt_new_copy_encoder(&enc_cfg, &g_enc), TAG, "rmt_new_copy_encoder");

    ESP_RETURN_ON_ERROR(rmt_enable(g_chan), TAG, "rmt_enable");

    ESP_LOGI(TAG, "WS2812 灯带初始化完成 (GPIO%d, %d 灯)", LED_BAR_PIN, LED_BAR_COUNT);
    return ESP_OK;
}

/* 构造并发送所有灯珠的时序 */
static void send_all(void)
{
    rmt_symbol_word_t items[LED_BAR_COUNT * BITS_PER_LED];
    int idx = 0;

    for (int led = 0; led < LED_BAR_COUNT; led++) {
        /* GRB 打包成 24 bit, 高位先发 */
        uint32_t bits = ((uint32_t)g_colors[led].g << 16) |
                        ((uint32_t)g_colors[led].r << 8) |
                        (uint32_t)g_colors[led].b;

        for (int b = BITS_PER_LED - 1; b >= 0; b--) {
            rmt_symbol_word_t sym = {0};
            if (bits & (1u << b)) {
                sym.duration0 = 8;  sym.level0 = 1;  /* 1码: 850ns 高 */
                sym.duration1 = 4;  sym.level1 = 0;  /*      400ns 低 */
            } else {
                sym.duration0 = 4;  sym.level0 = 1;  /* 0码: 400ns 高 */
                sym.duration1 = 8;  sym.level1 = 0;  /*      850ns 低 */
            }
            items[idx++] = sym;
        }
    }

    rmt_transmit_config_t txc = { .loop_count = 0 };
    rmt_transmit(g_chan, g_enc, items, sizeof(items), &txc);
    rmt_tx_wait_all_done(g_chan, portMAX_DELAY);
    vTaskDelay(pdMS_TO_TICKS(1));   /* WS2812 复位: >50us 低电平 */
}

/* 每键固定颜色（彩虹色，GRB 顺序；配对 1-9, 2-8, 3-7 同色） */
static const grb_t KEY_COLORS[LED_BAR_COUNT] = {
    {  0, 255,   0 },   /* key1 红    Do   */
    {128, 255,   0 },   /* key2 橙    Re   */
    {  0, 255, 255 },   /* key3 黄    Mi   */
    {255,   0,   0 },   /* key4 绿    Fa   */
    {255,   0, 255 },   /* key5 青    Sol  */
    {  0,   0, 255 },   /* key6 蓝    La   */
    {  0, 255, 255 },   /* key7 = key3 黄    Si   */
    {128, 255,   0 },   /* key8 = key2 橙    高Do */
    {  0, 255,   0 },   /* key9 = key1 红    高Re */
};

void led_bar_show_keys(uint16_t key_mask)
{
    /* 配对同步亮：1-9, 2-8, 3-7（同逻辑同色） */
    if (key_mask & (1u << 1)) key_mask |= (1u << 9);
    if (key_mask & (1u << 2)) key_mask |= (1u << 8);
    if (key_mask & (1u << 3)) key_mask |= (1u << 7);

    for (int i = 0; i < LED_BAR_COUNT; i++) {
        if (key_mask & (1u << (i + 1))) {
            g_colors[i] = KEY_COLORS[i];   /* 每个键固定颜色 */
        } else {
            g_colors[i].g = 0;  g_colors[i].r = 0;  g_colors[i].b = 0;   /* 灭 */
        }
    }
    send_all();
}

void led_bar_clear(void)
{
    led_bar_show_keys(0);
}
