// 架子鼓采样数据 — 自动生成
#pragma once
#include <stdint.h>
#define DRUM_SAMPLE_RATE 44100

extern const int16_t g_drum_1[];
extern const uint32_t g_drum_1_len;

extern const int16_t g_drum_2[];
extern const uint32_t g_drum_2_len;

extern const int16_t g_drum_3[];
extern const uint32_t g_drum_3_len;

extern const int16_t g_drum_4[];
extern const uint32_t g_drum_4_len;

extern const int16_t g_drum_5[];
extern const uint32_t g_drum_5_len;

extern const int16_t g_drum_6[];
extern const uint32_t g_drum_6_len;

extern const int16_t g_drum_7[];
extern const uint32_t g_drum_7_len;

extern const int16_t g_drum_8[];
extern const uint32_t g_drum_8_len;

extern const int16_t g_drum_9[];
extern const uint32_t g_drum_9_len;

typedef struct {
    const int16_t *data;
    uint32_t length;
} drum_sample_t;

extern const drum_sample_t g_drum_samples[9];
