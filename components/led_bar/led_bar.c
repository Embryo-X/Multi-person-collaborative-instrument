/**
 * ==============================================================
 * LED 灯带 — WS2812（官方 led_strip 组件，三条）
 *   条A: GPIO20, 3 灯 → 键 1~3   (RMT 后端)
 *   条B: GPIO21, 3 灯 → 键 4~6   (RMT 后端)
 *   条C: GPIO35, 3 灯 → 键 7~9   (SPI+DMA 后端, SPI3_HOST)
 * 亮起逻辑不变：bit(i+1) → 第 i 灯 (i=0..8)
 *
 * 为什么条C 用 SPI：ESP32-S3 的 RMT 只够 2 条 2 块内存的灯带，
 * 第 3 条用 SPI+DMA 后端（spi_bus_initialize 自动管理，堆内存不限 RMT 块）。
 * NV3007 屏已用 SPI2_HOST，故灯带用 SPI3_HOST。
 * ==============================================================
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_random.h"
#include "driver/spi_master.h"

#include "led_strip.h"
#include "led_bar.h"

static const char *TAG = "led_bar";

#define LED_BAR_PIN_A     GPIO_NUM_20   /* 条A: 键 1~3 */
#define LED_BAR_PIN_B     GPIO_NUM_21   /* 条B: 键 4~6 */
#define LED_BAR_PIN_C     GPIO_NUM_35   /* 条C: 键 7~9 */
#define LED_PER_STRIP    3
#define LED_BAR_RES_HZ   (10 * 1000 * 1000)   /* 10MHz RMT tick */

static led_strip_handle_t g_strip_a = NULL;
static led_strip_handle_t g_strip_b = NULL;
static led_strip_handle_t g_strip_c = NULL;

/* WS2812 颜色 (GRB 顺序) */
typedef struct { uint8_t g, r, b; } grb_t;
static grb_t g_colors[LED_BAR_COUNT];
static grb_t g_rand_colors[LED_BAR_COUNT];   /* 随机模式每键颜色（按下时分配一次） */

/* 创建一条 RMT 后端的灯带 */
static esp_err_t create_rmt_strip(int gpio, led_strip_handle_t *strip)
{
    led_strip_config_t led_cfg = {
        .strip_gpio_num = gpio,
        .max_leds = LED_PER_STRIP,
        .led_pixel_format = LED_PIXEL_FORMAT_GRB,
        .led_model = LED_MODEL_WS2812,
    };
    led_strip_rmt_config_t rmt_cfg = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = LED_BAR_RES_HZ,
        .mem_block_symbols = 96,       /* 3灯=72 symbols，2块(96)够；2条各2块放得下 */
        .flags.with_dma = false,
    };
    return led_strip_new_rmt_device(&led_cfg, &rmt_cfg, strip);
}

/* 创建一条 SPI+DMA 后端的灯带（第 3 条，绕开 RMT 内存块限制） */
static esp_err_t create_spi_strip(int gpio, led_strip_handle_t *strip)
{
    led_strip_config_t led_cfg = {
        .strip_gpio_num = gpio,
        .max_leds = LED_PER_STRIP,
        .led_pixel_format = LED_PIXEL_FORMAT_GRB,
        .led_model = LED_MODEL_WS2812,
    };
    led_strip_spi_config_t spi_cfg = {
        .clk_src = SPI_CLK_SRC_DEFAULT,
        .spi_bus = SPI3_HOST,          /* NV3007 已用 SPI2_HOST */
        .flags.with_dma = true,
    };
    return led_strip_new_spi_device(&led_cfg, &spi_cfg, strip);
}

esp_err_t led_bar_init(void)
{
    ESP_RETURN_ON_ERROR(create_rmt_strip(LED_BAR_PIN_A, &g_strip_a), TAG, "init strip A (RMT)");
    ESP_RETURN_ON_ERROR(create_rmt_strip(LED_BAR_PIN_B, &g_strip_b), TAG, "init strip B (RMT)");
    ESP_RETURN_ON_ERROR(create_spi_strip(LED_BAR_PIN_C, &g_strip_c), TAG, "init strip C (SPI+DMA)");

    ESP_LOGI(TAG, "WS2812 灯带初始化完成 (GPIO20:%d灯 键1~3 RMT, GPIO21:%d灯 键7~9 RMT, GPIO35:%d灯 键4~6 SPI+DMA)",
             LED_PER_STRIP, LED_PER_STRIP, LED_PER_STRIP);
    return ESP_OK;
}

/* 把 g_colors 三条灯带的颜色刷新到硬件 */
static void send_all(void)
{
    const led_strip_handle_t strips[3] = { g_strip_a, g_strip_b, g_strip_c };
    /* 物理条 → 全局键索引起点：
     *   A(GPIO20)=键1~3→0, B(GPIO21)=键4~6→3, C(GPIO35)=键7~9→6
     * （键序修正改在 keypad KEY_MAP，灯带保持原始分配） */
    const int base_idx[3] = { 0, 3, 6 };

    for (int s = 0; s < 3; s++) {
        if (strips[s] == NULL) continue;
        int base = base_idx[s];
        for (int j = 0; j < LED_PER_STRIP; j++) {
            grb_t *c = &g_colors[base + j];
            led_strip_set_pixel(strips[s], j, c->r, c->g, c->b);
        }
        led_strip_refresh(strips[s]);
    }
    vTaskDelay(pdMS_TO_TICKS(1));   /* WS2812 复位: >50us 低电平 */
}

/* 每键颜色 = Web keyColors.js 定色表（GRB），与谱面色块一致。
 * 与 song_data 选择一致：DEVICE_ROLE_STR 时用弦乐 STR 表，否则用打击 PERC 表。 */
#if defined(DEVICE_ROLE_STR)
/* 弦乐 STR_KEY_COLORS · keyId 0..7（8 色，第 9 灯保持灭） */
static const grb_t KEY_COLORS[8] = {
    {160, 232,  32},   /* keyId0 #e8a020 */
    {192, 240,  64},   /* keyId1 #f0c040 */
    {184,  18, 134},   /* keyId2 #12b886 */
    {201,  32, 151},   /* keyId3 #20c997 */
    {110,  76, 245},   /* keyId4 #4c6ef5 */
    {143, 116, 252},   /* keyId5 #748ffc */
    { 62, 174, 201},   /* keyId6 #ae3ec9 */
    {119, 218, 242},   /* keyId7 #da77f2 */
};
#define KEY_COLOR_COUNT 8
#else
/* 打击 PERC_KEY_COLORS · keyId 0..8（9 色） */
static const grb_t KEY_COLORS[9] = {
    {204, 255,   0},   /* keyId0 #ffcc00 */
    {255,   0, 102},   /* keyId1 #00ff66 */
    {204, 102, 255},   /* keyId2 #66ccff */
    {255, 170,   0},   /* keyId3 #aaff00 */
    {153, 255,   0},   /* keyId4 #ff9900 */
    {102, 255, 204},   /* keyId5 #ff66cc */
    { 51, 255,  51},   /* keyId6 #ff3333 */
    {160, 184, 255},   /* keyId7 #b8a0ff */
    {221,  68, 170},   /* keyId8 #44ddaa */
};
#define KEY_COLOR_COUNT 9
#endif

void led_bar_show_keys(uint16_t key_mask)
{
    for (int i = 0; i < LED_BAR_COUNT; i++) {
        if (key_mask & (1u << (i + 1))) {
            if (i < KEY_COLOR_COUNT) {
                g_colors[i] = KEY_COLORS[i];   /* 每个键固定颜色（Web 定色） */
            } else {
                g_colors[i].g = 0;  g_colors[i].r = 0;  g_colors[i].b = 0;  /* 超出色表则灭 */
            }
        } else {
            g_colors[i].g = 0;  g_colors[i].r = 0;  g_colors[i].b = 0;   /* 灭 */
        }
    }
    send_all();
}

/* 给某键分配一个随机 GRB 颜色（每次按下调用，颜色不固定） */
void led_bar_randomize_key(int key)
{
    if (key < 1 || key > LED_BAR_COUNT) return;
    grb_t *c = &g_rand_colors[key - 1];
    c->g = (uint8_t)(esp_random() >> 8);
    c->r = (uint8_t)(esp_random() >> 8);
    c->b = (uint8_t)(esp_random() >> 8);
}

/* 随机模式：按当前按下的键位掩码，用已分配的随机色点亮（松键即灭） */
void led_bar_show_keys_random(uint16_t key_mask)
{
    for (int i = 0; i < LED_BAR_COUNT; i++) {
        if (key_mask & (1u << (i + 1))) {
            g_colors[i] = g_rand_colors[i];   /* 用该键分配到的随机色 */
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
