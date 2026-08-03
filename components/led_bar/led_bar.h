/**
 * ==============================================================
 * LED 灯带 — WS2812 可寻址灯珠 (GPIO20)
 * 9 个灯珠对应矩阵键盘 9 音 (key 1~9)
 * ==============================================================
 */

#ifndef LED_BAR_H
#define LED_BAR_H

#include <stdint.h>
#include "esp_err.h"

#define LED_BAR_COUNT  9   /* 灯珠数 */

esp_err_t led_bar_init(void);

/* 按按键位掩码点亮灯珠：
 *   bit(i+1) 置位 → 第 i 个灯珠亮 (i = 0..8) */
void led_bar_show_keys(uint16_t key_mask);

void led_bar_clear(void);   /* 全灭 */

#endif
