/**
 * ==============================================================
 * 中文字库（16x16 点阵，宋体）
 * ==============================================================
 * 每字 32 字节：前 16 字节 = 上半部（8 行），后 16 字节 = 下半部（8 行）
 * 字节内高位在上（MSB = 顶部像素）
 *
 * 生成方式：freetype 渲染 Windows 宋体 16px
 * ==============================================================
 */

#ifndef CN_FONT_H
#define CN_FONT_H

#include <stdint.h>

/* 单个汉字字模（16x16 = 32 字节） */
typedef struct {
    uint8_t  utf8[3];       /* UTF-8 编码（3 字节） */
    const uint8_t *bitmap;  /* 指向 32 字节位图数据 */
} cn_char_t;

/* 字库中汉字的个数 */
extern const uint8_t CN_FONT_COUNT;

/* 字库数据（线性查找用） */
extern const cn_char_t cn_font_table[];

/**
 * @brief 按 UTF-8 字节查找汉字字模
 * @param b1   UTF-8 第 1 字节
 * @param b2   UTF-8 第 2 字节
 * @param b3   UTF-8 第 3 字节
 * @return 找到返回 bitmap 指针，否则返回 NULL
 */
const uint8_t *cn_font_lookup(uint8_t b1, uint8_t b2, uint8_t b3);

#endif /* CN_FONT_H */
