/**
 * ==============================================================
 * LED 指示灯组件 — 4 路 GPIO 对应 Do/Re/Mi/Fa
 * ==============================================================
 * GPIO16 = Do, GPIO17 = Re, GPIO18 = Mi, GPIO21 = Fa
 * 各串联 220Ω~1KΩ 限流电阻
 * ==============================================================
 */

#ifndef LED_H
#define LED_H

#include "esp_err.h"

#define LED_DO      1
#define LED_RE      2
#define LED_MI      3
#define LED_FA      4

esp_err_t led_init(void);
void led_on(int note);       /* 点亮指定音符 LED */
void led_off(int note);      /* 熄灭指定音符 LED */
void led_all_off(void);      /* 全灭 */

#endif
