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

/* ========== 分辨率 ==========
 * rotation 0 (竖屏): 142x428 */
#define NV3007_WIDTH      142
#define NV3007_HEIGHT     428
#define NV3007_X_OFFSET   12    /* rotation 0(0x00): X+12 */
#define NV3007_Y_OFFSET   0     /* Y 不偏移 */

/* ========== 扫描方向 MADCTL ==========
 * BadApple Arduino 驱动 rotation 映射:
 *   0: 0x00 (X+12,Y+0)   1: 0x60 (X+0,Y+12)
 *   2: 0xC0 (X+14,Y+0)   3: 0xA0 (X+0,Y+14) */
#define NV3007_MADCTL     0x00   /* rotation 0（竖屏，BadApple 官方验证） */

/* ========== API ========== */
esp_err_t nv3007_init(void);

/* 全屏填充一个颜色 */
void nv3007_fill_screen(uint16_t color);

/* 只刷新指定矩形区域 (坐标自动加 X 偏移) */
void nv3007_draw_region(int x0, int y0, int x1, int y1, const uint16_t *data);

/* 背光控制 */
void nv3007_backlight(bool on);

#endif
