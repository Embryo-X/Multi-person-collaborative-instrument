/**
 * ==============================================================
 * LED 灯带 — WS2812 可寻址灯珠（三条）
 *   条A: GPIO20, 3 灯 → 键 1~3
 *   条B: GPIO21, 3 灯 → 键 4~6
 *   条C: GPIO35, 3 灯 → 键 7~9
 * RMT 驱动，亮起逻辑不变 (bit(i+1) → 第 i 灯, i=0..8)
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

#define LED_BAR_PIN_A     GPIO_NUM_20   /* 条A: 键 1~3 */
#define LED_BAR_PIN_B     GPIO_NUM_21   /* 条B: 键 4~6 */
#define LED_BAR_PIN_C     GPIO_NUM_35   /* 条C: 键 7~9 */
#define LED_STRIP_A_COUNT 3
#define LED_STRIP_B_COUNT 3
#define LED_STRIP_C_COUNT 3
#define LED_BAR_RES_HZ    (10 * 1000 * 1000)   /* 100ns/tick */
#define BITS_PER_LED      24

static rmt_channel_handle_t g_chan_a = NULL;
static rmt_channel_handle_t g_chan_b = NULL;
static rmt_channel_handle_t g_chan_c = NULL;
static rmt_encoder_handle_t g_enc_a = NULL;
static rmt_encoder_handle_t g_enc_b = NULL;
static rmt_encoder_handle_t g_enc_c = NULL;

/* WS2812 颜色 (GRB 顺序) */
typedef struct { uint8_t g, r, b; } grb_t;
static grb_t g_colors[LED_BAR_COUNT];

/* 初始化一条 RMT 发送通道 */
static esp_err_t init_channel(rmt_channel_handle_t *chan, rmt_encoder_handle_t *enc, int gpio)
{
    rmt_tx_channel_config_t tx_cfg = {
        .gpio_num = gpio,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = LED_BAR_RES_HZ,
        .mem_block_symbols = 256,           /* 3*24=72 symbols 够用 */
        .trans_queue_depth = 4,
    };
    ESP_RETURN_ON_ERROR(rmt_new_tx_channel(&tx_cfg, chan), TAG, "rmt_new_tx_channel");

    rmt_copy_encoder_config_t enc_cfg = {};
    ESP_RETURN_ON_ERROR(rmt_new_copy_encoder(&enc_cfg, enc), TAG, "rmt_new_copy_encoder");

    ESP_RETURN_ON_ERROR(rmt_enable(*chan), TAG, "rmt_enable");
    return ESP_OK;
}

esp_err_t led_bar_init(void)
{
    ESP_RETURN_ON_ERROR(init_channel(&g_chan_a, &g_enc_a, LED_BAR_PIN_A), TAG, "init strip A");
    ESP_RETURN_ON_ERROR(init_channel(&g_chan_b, &g_enc_b, LED_BAR_PIN_B), TAG, "init strip B");
    ESP_RETURN_ON_ERROR(init_channel(&g_chan_c, &g_enc_c, LED_BAR_PIN_C), TAG, "init strip C");

    ESP_LOGI(TAG, "WS2812 灯带初始化完成 (GPIO20:%d灯 键1~3, GPIO21:%d灯 键4~6, GPIO35:%d灯 键7~9)",
             LED_STRIP_A_COUNT, LED_STRIP_B_COUNT, LED_STRIP_C_COUNT);
    return ESP_OK;
}

/* 把 g_colors[led_start .. led_start+led_count) 打包并发送到指定通道 */
static void send_strip(rmt_channel_handle_t chan, rmt_encoder_handle_t enc,
                       int led_start, int led_count)
{
    rmt_symbol_word_t items[led_count * BITS_PER_LED];
    int idx = 0;

    for (int led = led_start; led < led_start + led_count; led++) {
        /* GRB 打包成 24 bit, 高位先发 */
        uint32_t bits = ((uint32_t)g_colors[led].g << 16) |
                        ((uint32_t)g_colors[led].r << 8) |
                        (uint32_t)g_colors[led].b;

        for (int b = BITS_PER_LED - 1; b >= 0; b--) {
            rmt_symbol_word_t sym = {0};
            if (bits & (1u << b)) {
                sym.duration0 = 8;  sym.level0 = 1;   /* 1码: 850ns 高 */
                sym.duration1 = 4;  sym.level1 = 0;   /*      400ns 低 */
            } else {
                sym.duration0 = 4;  sym.level0 = 1;   /* 0码: 400ns 高 */
                sym.duration1 = 8;  sym.level1 = 0;   /*      850ns 低 */
            }
            items[idx++] = sym;
        }
    }

    rmt_transmit_config_t txc = { .loop_count = 0 };
    rmt_transmit(chan, enc, items, sizeof(items), &txc);
    rmt_tx_wait_all_done(chan, portMAX_DELAY);
}

/* 构造并发送三条灯带全部灯珠的时序 */
static void send_all(void)
{
    send_strip(g_chan_a, g_enc_a, 0,                     LED_STRIP_A_COUNT);
    send_strip(g_chan_b, g_enc_b, LED_STRIP_A_COUNT,     LED_STRIP_B_COUNT);
    send_strip(g_chan_c, g_enc_c, LED_STRIP_A_COUNT +
                                  LED_STRIP_B_COUNT,     LED_STRIP_C_COUNT);
    vTaskDelay(pdMS_TO_TICKS(1));   /* WS2812 复位: >50us 低电平 */
}

/* 每键固定颜色（彩虹色，GRB 顺序；高音区 7~9 与 3~1 同色） */
static const grb_t KEY_COLORS[LED_BAR_COUNT] = {
    {  0, 255,   0 },   /* key1 红    Do   */
    {128, 255,   0 },   /* key2 橙    Re   */
    {  0, 255, 255 },   /* key3 黄    Mi   */
    {255,   0,   0 },   /* key4 绿    Fa   */
    {255,   0, 255 },   /* key5 青    Sol  */
    {  0,   0, 255 },   /* key6 蓝    La   */
    {  0, 255, 255 },   /* key7 黄    Si   */
    {128, 255,   0 },   /* key8 橙    高Do */
    {  0, 255,   0 },   /* key9 红    高Re */
};

void led_bar_show_keys(uint16_t key_mask)
{
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
