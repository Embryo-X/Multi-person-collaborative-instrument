/**
 * ==============================================================
 * Touch 电容触摸 — 1 键 (高Mi)
 * ==============================================================
 * TOUCH14(GPIO14)=高Mi
 * 返回 10，与 keypad 的 1~9 不重叠
 */

#ifndef TOUCH_H
#define TOUCH_H
#include "esp_err.h"

/* 触摸键数 */
#define TOUCH_NUM      1

esp_err_t touch_init(void);

/* 返回: 0=无, 10=高Mi */
int touch_get_key(void);

#endif
