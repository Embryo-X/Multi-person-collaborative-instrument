/**
 * ==============================================================
 * 音频输出 — I2S MAX98357A + 真实钢琴采样回放
 * ==============================================================
 * 10个音符预采样 (物理建模生成): Do~HiMi
 * 每个采样完整播放，ADSR包络仅用于起音/释音平滑
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

static const char *TAG = "audio";

#define SAMPLE_RATE   44100

/* I2S 句柄 */
static i2s_chan_handle_t g_tx = NULL;

/* ADC 句柄 */
static adc_oneshot_unit_handle_t g_adc = NULL;

/* 共享状态 */
static const piano_sample_t *volatile g_sample = NULL;  /* 当前采样 */
static volatile uint32_t g_sample_pos = 0;               /* 播放位置 */
static volatile uint8_t  g_vol = 80;
static volatile int      g_note_on = 0;

/* 平滑音量（防电位器突变产生电音） */
static int g_vol_smooth = 80;

/* ==============================================================
 * 采样回放 + 轻量 ADSR（采样自带衰减，包络仅防爆音）
 * ============================================================== */
static void audio_task(void *arg)
{
    uint32_t pos = 0;
    int env = 0;
    enum { IDLE, PLAYING, RELEASE } state = IDLE;

    int16_t buf[256];

    while (1) {
        const piano_sample_t *sp = (const piano_sample_t *)g_sample;
        int note_on = g_note_on;
        g_note_on = 0;

        /* 音量滑动平均：每次向目标靠拢 1/8，消除电位器突变 */
        g_vol_smooth += ((int)g_vol - g_vol_smooth) / 8;
        int vol = g_vol_smooth;
        if (vol < 0) vol = 0;

        switch (state) {
        case IDLE:
            if (sp && sp->data && sp->length) {
                state = PLAYING;
                pos = 0;
                env = 0;
            }
            break;
        case PLAYING:
            /* ~4ms 起音斜坡，防止喀嚓声 */
            if (env < 256) env += 24;
            if (env > 256) env = 256;
            /* 采样放完 或 按键释放 或 采样被清除 */
            if (!sp || pos >= sp->length) {
                state = RELEASE;
            }
            if (note_on && sp) {
                /* 连奏：从头重新触发 */
                pos = 0;
                env = 0;
            }
            break;
        case RELEASE:
            env -= 8;   /* ~100ms 释音 */
            if (env <= 0) {
                env = 0;
                state = IDLE;
            }
            if (note_on && sp) {
                state = PLAYING;
                pos = 0;
                env = 0;
            }
            break;
        }

        if (state == IDLE || !sp) {
            memset(buf, 0, sizeof(buf));
        } else {
            uint32_t remain = sp->length - pos;
            for (int i = 0; i < 128; i++) {
                int32_t s = 0;
                if (i < (int)remain) {
                    s = (int32_t)sp->data[pos + i];
                }
                s = s * env * vol / 25600;
                /* 限幅余量 12000：MAX98357A 增益大，留 headroom 防破音 */
                if (s > 12000) s = 12000;
                if (s < -12000) s = -12000;
                buf[i * 2]     = (int16_t)s;
                buf[i * 2 + 1] = (int16_t)s;
            }
            pos += 128;
            if (pos > sp->length) pos = sp->length;
        }

        g_sample_pos = pos;

        size_t written = 0;
        i2s_channel_write(g_tx, buf, sizeof(buf), &written, portMAX_DELAY);
        taskYIELD();
    }
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
    for (int i = 0; i < 10; i++) {
        if (g_piano_samples[i].freq == freq_hz) {
            g_sample = &g_piano_samples[i];
            g_note_on = 1;
            return;
        }
    }
    /* 未找到匹配频率，不发声 */
    g_sample = NULL;
}

void audio_stop(void) { g_sample = NULL; }

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
