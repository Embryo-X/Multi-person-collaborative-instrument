/**
 * ==============================================================
 * TFT LCD 显示 — ST7735 128x160 SPI
 * ==============================================================
 */

#ifndef OLED_H
#define OLED_H

#include "driver/gpio.h"
#include "esp_err.h"

/* ========== SPI 引脚 ========== */
#define OLED_SPI_HOST           SPI2_HOST
#define OLED_SCLK_IO            GPIO_NUM_2
#define OLED_SDA_IO            GPIO_NUM_1
#define OLED_DC_IO              GPIO_NUM_42
#define OLED_RES_IO             GPIO_NUM_41
#define OLED_CS_IO              GPIO_NUM_40

/* --- 分辨率 --- */
#define OLED_WIDTH               128
#define OLED_HEIGHT              160

/* --- 字宽 --- */
#define FONT_ASC_WIDTH           6
#define FONT_CN_WIDTH            16

/* ========== API ========== */
esp_err_t oled_init(void);
void oled_clear(void);
void oled_draw_center_text(const char *text);
void oled_draw_text(uint8_t x, uint8_t y, const char *text);
void oled_refresh(void);
void oled_draw_vol(uint8_t pct);
void oled_draw_two_lines(const char *line1, const char *line2);  /* 双行居中 */

#endif
