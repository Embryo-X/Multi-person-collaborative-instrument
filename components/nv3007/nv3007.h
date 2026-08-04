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
#define NV3007_DC_IO      GPIO_NUM_41    /* 命令/数据 */
#define NV3007_RES_IO     GPIO_NUM_40    /* 复位 */
#define NV3007_CS_IO      GPIO_NUM_42    /* 片选 */
/* 背光 BLK 直接接 3.3V 常亮，不用 GPIO 控制 */

/* ========== 分辨率 ==========
 * rotation 1 (横屏, 1.txt 风格): 428x142 */
#define NV3007_WIDTH      428
#define NV3007_HEIGHT     142
#define NV3007_X_OFFSET   0     /* rotation 1(0x60): 偏移在Y轴 */
#define NV3007_Y_OFFSET   12    /* rotation 1(0x60): Y+12 */

/* ========== 扫描方向 MADCTL ==========
 * BadApple Arduino 驱动 rotation 映射:
 *   0: 0x00 (X+12,Y+0)   1: 0x60 (X+0,Y+12)
 *   2: 0xC0 (X+14,Y+0)   3: 0xA0 (X+0,Y+14) */
#define NV3007_MADCTL     0x60   /* rotation 1（横屏，1.txt 风格） */

/* ========== API ========== */
esp_err_t nv3007_init(void);

/* 全屏填充一个颜色 */
void nv3007_fill_screen(uint16_t color);

/* 只刷新指定矩形区域 (坐标自动加 X 偏移；内部做 RGB565 字节交换) */
void nv3007_draw_region(int x0, int y0, int x1, int y1, uint16_t *data);

/* ========== 三栏英文 UI（1.txt 风格，自带宽屏 fb + 6x8 字体缩放）========== */
void nv3007_ui_clear(void);
void nv3007_ui_fill_rect(int x, int y, int w, int h, uint16_t color);
void nv3007_ui_draw_vline(int x, int y, int h, uint16_t color);
void nv3007_ui_panel_text(const char *text, int16_t left, int16_t right,
                          int16_t y, uint8_t size, uint16_t color);
/* 左对齐文本（如左上角音量），写入帧缓冲，需 nv3007_ui_refresh() 生效 */
void nv3007_ui_text(int x, int y, const char *text, uint8_t size, uint16_t color);
void nv3007_ui_refresh(void);

#endif
