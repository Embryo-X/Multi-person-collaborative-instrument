/**
 * ==============================================================
 * 音频输出 — I2S MAX98357A 数字功放
 * ==============================================================
 * BCLK=48  LRCLK=45  DIN=47  音量ADC=10
 * ==============================================================
 */

#ifndef AUDIO_H
#define AUDIO_H

#include "esp_err.h"
#include <stdint.h>

/* I2S 引脚 */
#define AUDIO_BCLK_IO         GPIO_NUM_48
#define AUDIO_LRCLK_IO        GPIO_NUM_45
#define AUDIO_DIN_IO          GPIO_NUM_47

/* ADC 音量（GPIO10 = ADC1_CH9，ADC1 不受蓝牙干扰） */
#define VOLUME_ADC_PIN      10
#define VOLUME_ADC_UNIT     ADC_UNIT_1
#define VOLUME_ADC_CHAN     ADC_CHANNEL_9

/* 音高频率 (Hz) */
#define NOTE_DO             262
#define NOTE_RE             294
#define NOTE_MI             330
#define NOTE_FA             349
#define NOTE_SOL            392
#define NOTE_LA             440
#define NOTE_SI             494
#define NOTE_DO2            523
#define NOTE_RE2            587
#define NOTE_MI2            659
#define NOTE_HIFA           698
#define NOTE_HISOL          784
#define NOTE_HILA           880
#define NOTE_SILENCE        0

esp_err_t audio_init(void);
void audio_play_freq(uint32_t freq_hz);  /* 触发一个音符（钢琴，支持多音叠加） */
void audio_play_drum(int key);           /* 触发架子鼓音色（key 1~9） */
void audio_play_drum_key(int key);       /* BLE手动鼓（标记频率，可单独停止） */
void audio_stop_drum_key(int key);       /* 停止 BLE 手动鼓 */
void audio_play_gm_drum_midi(uint8_t midi);  /* 触发 GM 鼓音色（按 MIDI 音符查表） */
void audio_stop_freq(uint32_t freq_hz);  /* 停止指定音符 */
void audio_stop(void);                    /* 停止所有音符 */
void audio_set_volume(uint8_t pct);
void audio_update_volume(void);
uint8_t audio_get_volume(void);

#endif
