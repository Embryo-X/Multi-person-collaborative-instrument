/**
 * 板内 hardcode 时间轴引导（打击或弦乐由编译宏选头文件）
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "song_guide.h"
#include "led_bar.h"
#include "audio.h"

#include "song_data_believer_perc.h"
#include "song_data_qingchun_xiulian_perc.h"

static const char *TAG = "song_guide";

/* 歌曲注册表：song_guide_set_song(id) 选择播哪首（乐谱→Believer, BLE→青修） */
typedef struct {
    const song_event_t *events;
    uint32_t count;
    uint32_t duration_ms;
} song_def_t;

static const song_def_t g_songs[] = {
    { QINGCHUN_EVENTS, QINGCHUN_EVENT_COUNT, QINGCHUN_DURATION_MS },  /* 0 = 青春修炼手册 */
    { BELIEVER_EVENTS, BELIEVER_EVENT_COUNT, BELIEVER_DURATION_MS },  /* 1 = Believer */
};
static const song_def_t *g_song = &g_songs[0];

static volatile int g_playing = 0;
static TickType_t g_t0_tick = 0;
static uint32_t g_delay_ms = 0;
static int g_waiting_delay = 0;
static uint32_t g_evt_idx = 0;   /* 已发声的事件游标 */
static TaskHandle_t g_task = NULL;

/** keyId 0..8 → 朋友灯带 bit(keyId+1)；定色亮灭 */
static uint16_t mask_from_now(uint32_t now_ms)
{
    uint16_t mask = 0;
    /* 亮灯时长用每事件的 durationMs（与 Web 谱面色块一致），不再用固定 hold */
    for (uint32_t i = 0; i < g_song->count; i++) {
        uint32_t t = g_song->events[i].tMs;
        if (t > now_ms + 40) {
            break;
        }
        uint32_t end = t + g_song->events[i].durationMs;
        if (now_ms + 40 >= t && now_ms <= end) {
            uint8_t kid = g_song->events[i].keyId;
            if (kid < 9) {
                mask |= (uint16_t)(1u << (kid + 1));
            }
        }
    }
    return mask;
}

static void guide_task(void *arg)
{
    (void)arg;
    uint16_t last = 0xFFFF;
    while (1) {
        if (!g_playing) {
            if (last != 0) {
                led_bar_clear();
                last = 0;
            }
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }
        TickType_t now = xTaskGetTickCount();
        if (g_waiting_delay) {
            if ((now - g_t0_tick) < pdMS_TO_TICKS(g_delay_ms)) {
                vTaskDelay(pdMS_TO_TICKS(10));
                continue;
            }
            g_t0_tick = xTaskGetTickCount();
            g_waiting_delay = 0;
            ESP_LOGI(TAG, "timeline running");
        }
        uint32_t now_ms =
            (uint32_t)((now - g_t0_tick) * portTICK_PERIOD_MS);
        if (now_ms >= g_song->duration_ms) {
            ESP_LOGI(TAG, "song done @ %u ms", now_ms);
            g_playing = 0;
            led_bar_clear();
            last = 0;
            continue;
        }
        /* 事件时间到 → 播放对应 GM 鼓音色（每个事件触发一次，按事件 midi 发声） */
        while (g_evt_idx < g_song->count) {
            if (now_ms >= g_song->events[g_evt_idx].tMs) {
                audio_play_gm_drum_midi(g_song->events[g_evt_idx].midi);
                ESP_LOGI(TAG, "事件[%u] tMs=%u keyId=%u midi=%u", (unsigned)g_evt_idx,
                         (unsigned)g_song->events[g_evt_idx].tMs,
                         (unsigned)g_song->events[g_evt_idx].keyId,
                         (unsigned)g_song->events[g_evt_idx].midi);
                g_evt_idx++;
            } else break;
        }
        uint16_t mask = mask_from_now(now_ms);
        if (mask != last) {
            led_bar_show_keys(mask);
            last = mask;
        }
        vTaskDelay(pdMS_TO_TICKS(15));
    }
}

esp_err_t song_guide_init(void)
{
    ESP_LOGI(TAG, "歌曲注册表 %d 首，当前 events=%u duration=%u ms",
             (int)(sizeof(g_songs) / sizeof(g_songs[0])),
             (unsigned)g_song->count, (unsigned)g_song->duration_ms);
    if (!g_task) {
        xTaskCreate(guide_task, "song_guide", 4096, NULL, 5, &g_task);
    }
    return ESP_OK;
}

/* 选择要播放的歌曲：0=青修(青春修炼手册), 1=Believer */
void song_guide_set_song(int id)
{
    int n = (int)(sizeof(g_songs) / sizeof(g_songs[0]));
    if (id >= 0 && id < n) {
        g_song = &g_songs[id];
        ESP_LOGI(TAG, "选择歌曲 %d (%u 事件)", id, (unsigned)g_song->count);
    }
}

void song_guide_start(uint32_t delay_ms)
{
    g_delay_ms = delay_ms;
    g_t0_tick = xTaskGetTickCount();
    g_waiting_delay = delay_ms > 0 ? 1 : 0;
    if (!g_waiting_delay) {
        g_t0_tick = xTaskGetTickCount();
    }
    g_evt_idx = 0;   /* 重置事件游标 */
    g_playing = 1;
    ESP_LOGI(TAG, "START delay=%u ms", (unsigned)delay_ms);
}

void song_guide_stop(void)
{
    g_playing = 0;
    g_waiting_delay = 0;
    led_bar_clear();
    ESP_LOGI(TAG, "STOP");
}

int song_guide_is_playing(void)
{
    return g_playing;
}
