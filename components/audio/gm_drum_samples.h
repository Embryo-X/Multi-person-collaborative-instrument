// Believer GM 鼓音色（keyId 0-8）自动生成
#pragma once
#include <stdint.h>

extern const int16_t g_gm_drum_0[];
extern const uint32_t g_gm_drum_0_len;

extern const int16_t g_gm_drum_1[];
extern const uint32_t g_gm_drum_1_len;

extern const int16_t g_gm_drum_2[];
extern const uint32_t g_gm_drum_2_len;

extern const int16_t g_gm_drum_3[];
extern const uint32_t g_gm_drum_3_len;

extern const int16_t g_gm_drum_4[];
extern const uint32_t g_gm_drum_4_len;

extern const int16_t g_gm_drum_5[];
extern const uint32_t g_gm_drum_5_len;

extern const int16_t g_gm_drum_6[];
extern const uint32_t g_gm_drum_6_len;

extern const int16_t g_gm_drum_7[];
extern const uint32_t g_gm_drum_7_len;

extern const int16_t g_gm_drum_8[];
extern const uint32_t g_gm_drum_8_len;

typedef struct { const int16_t *data; uint32_t length; } gm_drum_t;
extern const gm_drum_t g_gm_drums[9];
