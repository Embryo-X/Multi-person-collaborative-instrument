// GM 鼓音色（按 MIDI 音符索引）自动生成
#pragma once
#include <stdint.h>

extern const int16_t g_gm_drum_36[];
extern const uint32_t g_gm_drum_36_len;

extern const int16_t g_gm_drum_45[];
extern const uint32_t g_gm_drum_45_len;

extern const int16_t g_gm_drum_46[];
extern const uint32_t g_gm_drum_46_len;

extern const int16_t g_gm_drum_38[];
extern const uint32_t g_gm_drum_38_len;

extern const int16_t g_gm_drum_37[];
extern const uint32_t g_gm_drum_37_len;

extern const int16_t g_gm_drum_51[];
extern const uint32_t g_gm_drum_51_len;

extern const int16_t g_gm_drum_39[];
extern const uint32_t g_gm_drum_39_len;

extern const int16_t g_gm_drum_42[];
extern const uint32_t g_gm_drum_42_len;

extern const int16_t g_gm_drum_49[];
extern const uint32_t g_gm_drum_49_len;

extern const int16_t g_gm_drum_35[];
extern const uint32_t g_gm_drum_35_len;

extern const int16_t g_gm_drum_57[];
extern const uint32_t g_gm_drum_57_len;

extern const int16_t g_gm_drum_43[];
extern const uint32_t g_gm_drum_43_len;

extern const int16_t g_gm_drum_41[];
extern const uint32_t g_gm_drum_41_len;

typedef struct { uint8_t midi; const int16_t *data; uint32_t length; } gm_drum_t;
extern const gm_drum_t g_gm_drums[13];
extern const int g_gm_drums_count;
