/**
 * ==============================================================
 * NV3007 液晶驱动 — 2.79" IPS 142x428 4-SPI
 * ==============================================================
 * 基于 esp_lcd_panel_io_spi（GPIO 矩阵任意引脚可用）
 * 初始化序列来自 ESPHome 社区验证版 (142x428, offset 12)
 * 若白屏/花屏需按屏厂 datasheet 校准
 * ==============================================================
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_lcd_panel_io.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"

#include "nv3007.h"

static const char *TAG = "nv3007";

static esp_lcd_panel_io_handle_t g_io = NULL;

/* ==============================================================
 * 初始化命令表 (厂商模式 0xFF 0xA5 → 配置 → 0xFF 0x00 退出)
 * ============================================================== */
typedef struct { uint8_t cmd; uint8_t len; uint8_t data[8]; } init_cmd_t;

/* 官方 WH-279-05 (2.79寸 142x428) 初始化序列，照抄 STM32 例程 */
static const init_cmd_t NV3007_INIT[] = {
    {0xFF,1,{0xA5}},                    /* 进入厂商模式 */
    {0x9A,1,{0x08}}, {0x9B,1,{0x08}}, {0x9C,1,{0xB0}}, {0x9D,1,{0x16}}, {0x9E,1,{0xC4}},
    {0x8F,2,{0x55,0x04}}, {0x84,1,{0x90}}, {0x83,1,{0x7B}}, {0x85,1,{0x33}},
    {0x60,1,{0x00}}, {0x70,1,{0x00}}, {0x61,1,{0x02}}, {0x71,1,{0x02}},
    {0x62,1,{0x04}}, {0x72,1,{0x04}}, {0x6C,1,{0x29}}, {0x7C,1,{0x29}},
    {0x6D,1,{0x31}}, {0x7D,1,{0x31}}, {0x6E,1,{0x0F}}, {0x7E,1,{0x0F}},
    {0x66,1,{0x21}}, {0x76,1,{0x21}}, {0x68,1,{0x3A}}, {0x78,1,{0x3A}},
    {0x63,1,{0x07}}, {0x73,1,{0x07}}, {0x64,1,{0x05}}, {0x74,1,{0x05}},
    {0x65,1,{0x02}}, {0x75,1,{0x02}}, {0x67,1,{0x23}}, {0x77,1,{0x23}},
    {0x69,1,{0x08}}, {0x79,1,{0x08}}, {0x6A,1,{0x13}}, {0x7A,1,{0x13}},
    {0x6B,1,{0x13}}, {0x7B,1,{0x13}}, {0x6F,1,{0x00}}, {0x7F,1,{0x00}},
    /* 时序参数 */
    {0x50,1,{0x00}}, {0x52,1,{0xD6}}, {0x53,1,{0x08}}, {0x54,1,{0x08}}, {0x55,1,{0x1E}}, {0x56,1,{0x1C}},
    /* GOA map sel */
    {0xA0,3,{0x2B,0x24,0x00}}, {0xA1,1,{0x87}}, {0xA2,1,{0x86}},
    {0xA5,1,{0x00}}, {0xA6,1,{0x00}}, {0xA7,1,{0x00}},
    {0xA8,1,{0x36}}, {0xA9,1,{0x7E}}, {0xAA,1,{0x7E}},
    {0xB9,1,{0x85}}, {0xBA,1,{0x84}}, {0xBB,1,{0x83}}, {0xBC,1,{0x82}},
    {0xBD,1,{0x81}}, {0xBE,1,{0x80}}, {0xBF,1,{0x01}}, {0xC0,1,{0x02}},
    {0xC1,1,{0x00}}, {0xC2,1,{0x00}}, {0xC3,1,{0x00}}, {0xC4,1,{0x33}},
    {0xC5,1,{0x7E}}, {0xC6,1,{0x7E}},
    {0xC8,2,{0x33,0x33}}, {0xC9,1,{0x68}}, {0xCA,1,{0x69}}, {0xCB,1,{0x6A}}, {0xCC,1,{0x6B}},
    {0xCD,2,{0x33,0x33}}, {0xCE,1,{0x6C}}, {0xCF,1,{0x6D}},
    {0xD0,1,{0x6E}}, {0xD1,1,{0x6F}},
    {0xAB,2,{0x03,0x67}}, {0xAC,2,{0x03,0x6B}}, {0xAD,2,{0x03,0x68}}, {0xAE,2,{0x03,0x6C}},
    {0xB3,1,{0x00}}, {0xB4,1,{0x00}}, {0xB5,1,{0x00}},
    {0xB6,1,{0x32}}, {0xB7,1,{0x7E}}, {0xB8,1,{0x7E}},
    /* 色彩校正 Gamma */
    {0xE0,1,{0x00}}, {0xE1,2,{0x03,0x0F}}, {0xE2,1,{0x04}}, {0xE3,1,{0x01}},
    {0xE4,1,{0x0E}}, {0xE5,1,{0x01}}, {0xE6,1,{0x19}}, {0xE7,1,{0x10}},
    {0xE8,1,{0x10}}, {0xEA,1,{0x12}}, {0xEB,1,{0xD0}}, {0xEC,1,{0x04}},
    {0xED,1,{0x07}}, {0xEE,1,{0x07}}, {0xEF,1,{0x09}},
    {0xF0,1,{0xD0}}, {0xF1,2,{0x0E,0x17}}, {0xF2,4,{0x2C,0x1B,0x0B,0x20}},
    {0xE9,1,{0x29}}, {0xEC,1,{0x04}},
    /* TE */
    {0x35,1,{0x00}}, {0x44,2,{0x00,0x10}}, {0x46,1,{0x10}},
    /* 退出厂商模式 */
    {0xFF,1,{0x00}},
};
#define NV3007_INIT_COUNT  (sizeof(NV3007_INIT) / sizeof(NV3007_INIT[0]))

/* ==============================================================
 * 底层命令发送
 * ============================================================== */
static void nv3007_cmd(uint8_t cmd, const uint8_t *data, uint8_t len)
{
    esp_lcd_panel_io_tx_param(g_io, cmd, data, len);
}

/* ==============================================================
 * 初始化
 * ============================================================== */
esp_err_t nv3007_init(void)
{
    ESP_LOGI(TAG, "初始化 NV3007 %dx%d 4-SPI...", NV3007_WIDTH, NV3007_HEIGHT);

    /* 背光 */
    gpio_reset_pin(NV3007_BL_IO);
    gpio_set_direction(NV3007_BL_IO, GPIO_MODE_OUTPUT);
    gpio_set_level(NV3007_BL_IO, 1);

    /* SPI 总线 */
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = NV3007_MOSI_IO,
        .miso_io_num = -1,
        .sclk_io_num = NV3007_SCLK_IO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = NV3007_WIDTH * NV3007_HEIGHT * 2 + 10,
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(SPI2_HOST, &bus_cfg, SPI_DMA_CH_AUTO),
                        TAG, "spi_bus_initialize");

    /* SPI Panel IO */
    esp_lcd_panel_io_spi_config_t io_cfg = {
        .cs_gpio_num = NV3007_CS_IO,
        .dc_gpio_num = NV3007_DC_IO,
        .spi_mode = 0,
        .pclk_hz = 20 * 1000 * 1000,
        .trans_queue_depth = 4,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST,
                                                 &io_cfg, &g_io),
                        TAG, "esp_lcd_new_panel_io_spi");

    /* 硬件复位 */
    gpio_reset_pin(NV3007_RES_IO);
    gpio_set_direction(NV3007_RES_IO, GPIO_MODE_OUTPUT);
    gpio_set_level(NV3007_RES_IO, 0);
    vTaskDelay(pdMS_TO_TICKS(20));
    gpio_set_level(NV3007_RES_IO, 1);
    vTaskDelay(pdMS_TO_TICKS(120));

    /* 初始化命令序列 */
    for (uint32_t i = 0; i < NV3007_INIT_COUNT; i++)
        nv3007_cmd(NV3007_INIT[i].cmd, NV3007_INIT[i].data, NV3007_INIT[i].len);

    /* Sleep Out */
    esp_lcd_panel_io_tx_param(g_io, 0x11, NULL, 0);
    vTaskDelay(pdMS_TO_TICKS(200));

    /* 像素格式 RGB565（官方: 0x05） */
    uint8_t colmod = 0x05;
    esp_lcd_panel_io_tx_param(g_io, 0x3A, &colmod, 1);

    /* 扫描方向 MADCTL */
    uint8_t madctl = NV3007_MADCTL;
    esp_lcd_panel_io_tx_param(g_io, 0x36, &madctl, 1);

    /* 厂商核心窗口 */
    uint8_t colwin[4] = { 0x00, 0x0C, 0x00, 0x99 };   /* X 12-153 (142px) */
    uint8_t rowwin[4] = { 0x00, 0x00, 0x01, 0xAB };   /* Y 0-427 (428px) */
    nv3007_cmd(0x2A, colwin, 4);
    nv3007_cmd(0x2B, rowwin, 4);

    /* Display On */
    esp_lcd_panel_io_tx_param(g_io, 0x29, NULL, 0);
    vTaskDelay(pdMS_TO_TICKS(150));

    ESP_LOGI(TAG, "NV3007 初始化完成 (MADCTL=0x%02X)", madctl);
    return ESP_OK;
}

/* ==============================================================
 * 区域刷新（坐标自动加 X 偏移）
 * ============================================================== */
void nv3007_draw_region(int x0, int y0, int x1, int y1, const uint16_t *data)
{
    uint8_t col[4] = { (uint8_t)((x0 + NV3007_X_OFFSET) >> 8), (uint8_t)((x0 + NV3007_X_OFFSET) & 0xFF),
                       (uint8_t)((x1 + NV3007_X_OFFSET) >> 8), (uint8_t)((x1 + NV3007_X_OFFSET) & 0xFF) };
    uint8_t row[4] = { (uint8_t)(y0 >> 8), (uint8_t)(y0 & 0xFF),
                       (uint8_t)(y1 >> 8), (uint8_t)(y1 & 0xFF) };
    nv3007_cmd(0x2A, col, 4);
    nv3007_cmd(0x2B, row, 4);
    esp_lcd_panel_io_tx_color(g_io, 0x2C, data, (x1 - x0 + 1) * (y1 - y0 + 1) * 2);
}

void nv3007_fill_screen(uint16_t color)
{
    /* 用一个小缓冲逐行填充，避免超大内存 */
    uint16_t line[NV3007_WIDTH];
    for (int x = 0; x < NV3007_WIDTH; x++) line[x] = color;
    for (int y = 0; y < NV3007_HEIGHT; y++)
        nv3007_draw_region(0, y, NV3007_WIDTH - 1, y, line);
}

void nv3007_backlight(bool on)
{
    gpio_set_level(NV3007_BL_IO, on ? 1 : 0);
}

