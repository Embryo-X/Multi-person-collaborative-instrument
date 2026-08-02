/**
 * ==============================================================
 * NV3007 液晶驱动 — 2.79" IPS 142x428 4-SPI
 * ==============================================================
 * 独立驱动组件，基于 esp_lcd_panel_io_spi
 * 引脚/分辨率/扫描方向集中在此头文件修改
 * ==============================================================
 */

#ifndef NV3007_H
#define NV3007_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/gpio.h"

/* ========== 引脚 ========== */
#define NV3007_SCLK_IO    GPIO_NUM_2     /* SPI 时钟 */
#define NV3007_MOSI_IO    GPIO_NUM_1     /* SPI 数据 (屏丝印 MOS) */
#define NV3007_DC_IO      GPIO_NUM_42    /* 命令/数据 */
#define NV3007_RES_IO     GPIO_NUM_41    /* 复位 */
#define NV3007_CS_IO      GPIO_NUM_40    /* 片选 */
#define NV3007_BL_IO      GPIO_NUM_9     /* 背光 */

/* ========== 分辨率 ========== */
#define NV3007_WIDTH      142
#define NV3007_HEIGHT     428
#define NV3007_X_OFFSET   14    /* 厂商窗口 X 偏移（方向 0xC0 时为 14） */

/* ========== 扫描方向 MADCTL ==========
 * 官方 WH-279-05 例程: USE_HORIZONTAL
 *   0 → 0x00 (正常)  1 → 0xC0 (180°翻转)
 *   2 → 0x60 (90°)   3 → 0xA0 (270°)
 * 对应窗口偏移: 0→X+12  1→X+14  2→Y+14  3→Y+12 */
#define NV3007_MADCTL     0xC0   /* 180°翻转 */

/* ========== API ========== */
esp_err_t nv3007_init(void);

/* 全屏填充一个颜色 */
void nv3007_fill_screen(uint16_t color);

/* 只刷新指定矩形区域 (坐标自动加 X 偏移) */
void nv3007_draw_region(int x0, int y0, int x1, int y1, const uint16_t *data);

/* 背光控制 */
void nv3007_backlight(bool on);

#endif
