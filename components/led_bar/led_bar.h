/**
 * ==============================================================
 * LED 灯带 — WS2812 可寻址灯珠（双条）
 *   条A: GPIO20, 3 灯 → 键 1~3
 *   条B: GPIO21, 3 灯 → 键 4~6
 * 亮起逻辑不变：bit(i+1) 置位 → 第 i 个灯珠亮 (i = 0..5)
 * ==============================================================
 */

#ifndef LED_BAR_H
#define LED_BAR_H

#include <stdint.h>
#include "esp_err.h"

#define LED_BAR_COUNT  6   /* 灯珠总数（条A 3 + 条B 3） */

esp_err_t led_bar_init(void);

/* 按按键位掩码点亮灯珠：
 *   bit(i+1) 置位 → 第 i 个灯珠亮 (i = 0..5, 键 1..6)
 *   键 1~3 → GPIO20 条前 3 灯；键 4~6 → GPIO21 条 3 灯 */
void led_bar_show_keys(uint16_t key_mask);

void led_bar_clear(void);   /* 全灭 */

#endif
