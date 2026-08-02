/**
 * ==============================================================
 * 矩阵键盘 — 3 行 × 3 列 = 9 键
 * ==============================================================
 * 行: GPIO38/39/21 (输出)  列: GPIO35/36/37 (输入+上拉)
 *       C35   C36   C37
 * R38    Do    Re    Mi
 * R39    Fa    Sol   La
 * R21    Si   高Do  高Re
 * ==============================================================
 */

#ifndef KEYPAD_H
#define KEYPAD_H

#include "esp_err.h"

esp_err_t keypad_init(void);

/* 0=无, 1=Do, 2=Re, 3=Mi, 4=Fa, 5=Sol, 6=La, 7=Si, 8=高Do, 9=高Re */
int keypad_get_key(void);

/* 返回当前所有按下的键位掩码（bit k = 1 表示 key k 被按下），支持多键 */
uint16_t keypad_get_pressed_mask(void);

/* 测试用：行列号 (0-based)，无按键返回 -1 */
int keypad_get_row(void);
int keypad_get_col(void);

#endif
