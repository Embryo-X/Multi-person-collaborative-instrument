// 真实钢琴采样数据 — 88键钢琴录音
#pragma once
#include <stdint.h>

#define PIANO_SAMPLE_RATE 44100

extern const int16_t g_sample_do[];
extern const uint32_t g_sample_do_len;

extern const int16_t g_sample_re[];
extern const uint32_t g_sample_re_len;

extern const int16_t g_sample_mi[];
extern const uint32_t g_sample_mi_len;

extern const int16_t g_sample_fa[];
extern const uint32_t g_sample_fa_len;

extern const int16_t g_sample_sol[];
extern const uint32_t g_sample_sol_len;

extern const int16_t g_sample_la[];
extern const uint32_t g_sample_la_len;

extern const int16_t g_sample_si[];
extern const uint32_t g_sample_si_len;

extern const int16_t g_sample_hido[];
extern const uint32_t g_sample_hido_len;

extern const int16_t g_sample_hire[];
extern const uint32_t g_sample_hire_len;

extern const int16_t g_sample_himi[];
extern const uint32_t g_sample_himi_len;

typedef struct {
    const int16_t *data;
    uint32_t length;
    uint32_t freq;
} piano_sample_t;

extern const piano_sample_t g_piano_samples[10];
