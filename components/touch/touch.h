/**
 * ==============================================================
 * Touch 电容触摸 — 4 键 (高Mi / 高Fa / 高Sol / 高La)
 * ==============================================================
 * TOUCH11(GPIO11)=高Mi  TOUCH12(GPIO12)=高Fa
 * TOUCH13(GPIO13)=高Sol  TOUCH14(GPIO14)=高La
 * 返回 10~13，与 keypad 的 1~9 不重叠
 */

#ifndef TOUCH_H
#define TOUCH_H
#include "esp_err.h"

/* 触摸键数 */
#define TOUCH_NUM      4

esp_err_t touch_init(void);

/* 返回: 0=无, 10=高Mi, 11=高Fa, 12=高Sol, 13=高La */
int touch_get_key(void);

/* 调试：打印各触摸通道的原始读数（电压变化） */
void touch_print_raw(void);

#endif
