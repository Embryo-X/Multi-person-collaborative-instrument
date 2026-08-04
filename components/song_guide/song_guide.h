/**
 * 板内 hardcode 时间轴：START 后按 tMs 亮灯（REQ-020 / H1）
 */
#pragma once

#include "esp_err.h"
#include <stdint.h>

esp_err_t song_guide_init(void);
/** Web Control START：delayMs 后本地时钟归零开跑 */
void song_guide_start(uint32_t delay_ms);
void song_guide_stop(void);
int song_guide_is_playing(void);
/** 选择播放的歌曲：0=青修(青春修炼手册), 1=Believer */
void song_guide_set_song(int id);
/** 选择事件发声乐器：0=架子鼓(GM鼓), 1=钢琴(占位音) */
void song_guide_set_instrument(int mode);
