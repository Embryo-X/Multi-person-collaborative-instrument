/**
 * ==============================================================
 * OLED 绘制层 — NV3007 142x428 屏幕
 * 底层 SPI/初始化由独立 nv3007 组件负责
 * 本组件负责帧缓冲 + 中文/ASCII 绘制
 * ==============================================================
 */

#ifndef OLED_H
#define OLED_H

#include <stdint.h>
#include "esp_err.h"

/* --- 分辨率（与 nv3007 组件一致，rotation 1 横屏） --- */
#define OLED_WIDTH       142
#define OLED_HEIGHT      428

/* --- 字宽 --- */
#define FONT_ASC_WIDTH   6
#define FONT_CN_WIDTH    16

/* ========== API ========== */
esp_err_t oled_init(void);
void oled_clear(void);
void oled_draw_center_text(const char *text);
void oled_draw_text(uint8_t x, uint8_t y, const char *text);
void oled_refresh(void);
void oled_draw_vol(uint8_t pct);
void oled_draw_two_lines(const char *line1, const char *line2);  /* 双行居中 */

#endif
