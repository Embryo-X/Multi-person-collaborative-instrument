/**
 * ==============================================================
 * 音频输出 — I2S MAX98357A + 真实钢琴采样多声道回放
 * ==============================================================
 * 10个音符预采样: Do~HiMi
 * 8 声道 voice 池：每声道独立采样/位置/包络，
 * 支持同时按下多个键 → 和弦/重叠音叠加输出
 * ==============================================================
 */

#include <string.h>
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "driver/i2s_std.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "audio.h"
#include "piano_samples.h"
#include "drum_samples.h"
#include "gm_drum_samples.h"

static const char *TAG = "audio";

#define SAMPLE_RATE   44100
#define MAX_VOICES    8          /* 最多同时响 8 个音 */

/* I2S 句柄 */
static i2s_chan_handle_t g_tx = NULL;

/* ADC 句柄 */
static adc_oneshot_unit_handle_t g_adc = NULL;

static volatile uint8_t g_vol = 80;

/* 平滑音量（防电位器突变产生电音） */
static int g_vol_smooth = 80;

/* ---- 多声道 voice ----
 * state: 0=idle 1=attack 2=sustain 3=release
 * 通用采样（钢琴/鼓共用），pos_fixed/step 支持变调 */
typedef struct {
    const int16_t *data;            /* 采样数据（NULL=空闲） */
    uint32_t length;                /* 采样长度 */
    uint32_t freq;                  /* 目标频率（stop_freq 匹配用），鼓=0 */
    uint32_t pos_fixed;             /* 16.16 定点采样位置 */
    uint32_t step;                  /* 16.16 定点步进（1.0 = 原速） */
    int      env;                   /* 包络 0~256 */
    int      state;
} voice_t;

static voice_t g_voices[MAX_VOICES];
static uint32_t g_voice_next = 0;   /* 全忙时的轮转替换指针 */

/* ==============================================================
 * 多声道混音回放
 * ============================================================== */
static void audio_task(void *arg)
{
    int16_t buf[256];

    while (1) {
        /* 音量滑动平均：每次向目标靠拢 1/8，消除电位器突变 */
        g_vol_smooth += ((int)g_vol - g_vol_smooth) / 8;
        int vol = g_vol_smooth;
        if (vol < 0) vol = 0;

        /* ---- 每块更新各 voice 状态机 ---- */
        for (int v = 0; v < MAX_VOICES; v++) {
            voice_t *vo = &g_voices[v];
            switch (vo->state) {
            case 1: /* attack: ~4ms 起音斜坡防爆音 */
                vo->env += 24;
                if (vo->env >= 256) { vo->env = 256; vo->state = 2; }
                break;
            case 2: /* sustain: 采样放完或已释放 → release */
                if (!vo->data || (vo->pos_fixed >> 16) >= vo->length)
                    vo->state = 3;
                break;
            case 3: /* release: ~100ms 释音 */
                vo->env -= 8;
                if (vo->env <= 0) {
                    vo->env = 0;
                    vo->state = 0;
                    vo->data = NULL;
                }
                break;
            default:
                break;
            }
        }

        /* ---- 混合 128 采样（每声道独立位置，支持变调步进） ---- */
        memset(buf, 0, sizeof(buf));
        for (int v = 0; v < MAX_VOICES; v++) {
            voice_t *vo = &g_voices[v];
            if (vo->state == 0 || !vo->data) continue;
            const int16_t *data = vo->data;
            uint32_t len = vo->length;
            uint32_t step = vo->step;

            for (int i = 0; i < 128; i++) {
                uint32_t idx = vo->pos_fixed >> 16;
                if (idx >= len) break;
                int32_t s = (int32_t)data[idx] * vo->env * vol / 25600;
                vo->pos_fixed += step;
                /* 叠加到总线，限幅余量 12000 防破音 */
                int32_t mix = (int32_t)buf[i * 2] + s;
                if (mix > 12000) mix = 12000;
                if (mix < -12000) mix = -12000;
                buf[i * 2]     = (int16_t)mix;
                buf[i * 2 + 1] = (int16_t)mix;
            }
        }

        size_t written = 0;
        i2s_channel_write(g_tx, buf, sizeof(buf), &written, portMAX_DELAY);
        taskYIELD();
    }
}

/* 分配一个 voice 播放指定采样（step=16.16 定点播放步进） */
static voice_t *voice_allocate(const int16_t *data, uint32_t length, uint32_t freq, uint32_t step)
{
    if (step == 0) step = 1;
    /* 1. 重触发同频 voice（连奏） */
    for (int v = 0; v < MAX_VOICES; v++) {
        if (g_voices[v].freq == freq && g_voices[v].state > 0 && freq != 0) {
            g_voices[v].data = data;
            g_voices[v].length = length;
            g_voices[v].pos_fixed = 0;
            g_voices[v].step = step;
            g_voices[v].env = 0;
            g_voices[v].state = 1;
            return &g_voices[v];
        }
    }
    /* 2. 找空闲 voice */
    for (int v = 0; v < MAX_VOICES; v++) {
        if (g_voices[v].state == 0) {
            g_voices[v].data = data;
            g_voices[v].length = length;
            g_voices[v].freq = freq;
            g_voices[v].pos_fixed = 0;
            g_voices[v].step = step;
            g_voices[v].env = 0;
            g_voices[v].state = 1;
            return &g_voices[v];
        }
    }
    /* 3. 全忙：轮转替换（新的优先于最旧） */
    uint32_t v = g_voice_next++ % MAX_VOICES;
    g_voices[v].data = data;
    g_voices[v].length = length;
    g_voices[v].freq = freq;
    g_voices[v].pos_fixed = 0;
    g_voices[v].step = step;
    g_voices[v].env = 0;
    g_voices[v].state = 1;
    return &g_voices[v];
}

/* ==============================================================
 * 公开 API
 * ============================================================== */

esp_err_t audio_init(void)
{
    ESP_LOGI(TAG, "初始化 I2S MAX98357A (BCLK=48 LRCLK=45 DIN=47)...");

    /* --- I2S 通道 ---
     * 大 DMA 缓冲 (8 帧 x 1024 采样) 防止 underflow 产生杂音 */
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num = 8;
    chan_cfg.dma_frame_num = 1024;
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &g_tx, NULL));

    i2s_std_config_t std_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = AUDIO_BCLK_IO,
            .ws   = AUDIO_LRCLK_IO,
            .dout = AUDIO_DIN_IO,
            .din  = I2S_GPIO_UNUSED,
        },
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(g_tx, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(g_tx));

    /* --- ADC 音量 --- */
    adc_oneshot_unit_init_cfg_t adc_cfg = {
        .unit_id = VOLUME_ADC_UNIT,
        .clk_src = ADC_DIGI_CLK_SRC_DEFAULT,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&adc_cfg, &g_adc));
    adc_oneshot_chan_cfg_t ch = { .atten = ADC_ATTEN_DB_12, .bitwidth = ADC_BITWIDTH_12 };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(g_adc, VOLUME_ADC_CHAN, &ch));

    unsigned long total = (unsigned long)(g_sample_do_len + g_sample_re_len + g_sample_mi_len
        + g_sample_fa_len + g_sample_sol_len + g_sample_la_len
        + g_sample_si_len + g_sample_hido_len + g_sample_hire_len
        + g_sample_himi_len) * 2;
    ESP_LOGI(TAG, "已加载 10 个钢琴采样, 总计 %lu 字节", total);

    /* --- 启动回放任务 ---
     * 优先级 12：高于主循环(1)，保证 I2S 及时填充不 underflow。
     * 低于蓝牙 host 任务，不会干扰 BLE。 */
    xTaskCreate(audio_task, "audio", 4096, NULL, 12, NULL);

    ESP_LOGI(TAG, "I2S 音频初始化完成");
    return ESP_OK;
}

void audio_play_freq(uint32_t freq_hz)
{
    /* 按频率查找采样 */
    const piano_sample_t *sp = NULL;
    uint32_t sample_freq = freq_hz;   /* 默认 1:1 */
    for (int i = 0; i < 10; i++) {
        if (g_piano_samples[i].freq == freq_hz) {
            sp = &g_piano_samples[i];
            sample_freq = g_piano_samples[i].freq;
            break;
        }
    }
    if (!sp) {
        /* 无匹配采样（高Fa/高Sol/高La）：用最高音(高Mi) 变调播放 */
        sp = &g_piano_samples[9];
        sample_freq = g_piano_samples[9].freq;
    }

    /* 16.16 定点播放步进 = 目标频率 / 采样频率 */
    uint32_t step = (uint32_t)(((uint64_t)freq_hz << 16) / sample_freq);
    voice_allocate(sp->data, sp->length, freq_hz, step);
}

void audio_play_drum(int key)
{
    /* 架子鼓：key 1~9 对应 drum_samples[0..8]，原速播放 */
    if (key < 1 || key > 9) return;
    const drum_sample_t *d = &g_drum_samples[key - 1];
    voice_allocate(d->data, d->length, 0, 0x10000);   /* step 1.0 */
}

void audio_play_gm_drum_midi(uint8_t midi)
{
    /* GM 鼓：按 MIDI 音符在 g_gm_drums[] 查表，原速播放 */
    for (int i = 0; i < g_gm_drums_count; i++) {
        if (g_gm_drums[i].midi == midi) {
            const gm_drum_t *d = &g_gm_drums[i];
            voice_allocate(d->data, d->length, 0, 0x10000);
            return;
        }
    }
    ESP_LOGW(TAG, "GM鼓 midi=%d 无采样，忽略", (int)midi);
}

void audio_stop_freq(uint32_t freq_hz)
{
    /* 停止所有同频 voice（进入释音，平滑结束） */
    for (int v = 0; v < MAX_VOICES; v++) {
        if (g_voices[v].freq == freq_hz && g_voices[v].state > 0)
            g_voices[v].state = 3;
    }
}

void audio_stop(void)
{
    /* 停止所有 voice */
    for (int v = 0; v < MAX_VOICES; v++) {
        if (g_voices[v].state > 0)
            g_voices[v].state = 3;
    }
}

void audio_set_volume(uint8_t pct) { if (pct <= 100) g_vol = pct; }

void audio_update_volume(void)
{
    int raw = 0;
    if (adc_oneshot_read(g_adc, VOLUME_ADC_CHAN, &raw) == ESP_OK) {
        g_vol = (uint8_t)((uint32_t)raw * 100 / 4095);
        ESP_LOGI(TAG, "ADC=%d → 音量=%d%%", raw, (int)g_vol);
    }
}

uint8_t audio_get_volume(void) { return g_vol; }
