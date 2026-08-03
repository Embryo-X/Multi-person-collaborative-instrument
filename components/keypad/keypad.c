/**
 * ==============================================================
 * 矩阵键盘 — 3×3 扫描 + 消抖
 * ==============================================================
 * 行: GPIO38/39/21  列: GPIO35/36/37
 *     C35  C36  C37
 * R38  Do   Re   Mi
 * R39  Fa   Sol  La
 * R21  Si  高Do 高Re
 * ==============================================================
 */

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "rom/ets_sys.h"
#include "keypad.h"

static const char *TAG = "keypad";

#define ROWS         3
#define COLS         3
#define DEBOUNCE_US  30000

static const int ROW_PINS[ROWS] = { GPIO_NUM_7, GPIO_NUM_6, GPIO_NUM_5 };
static const int COL_PINS[COLS] = { GPIO_NUM_17, GPIO_NUM_16, GPIO_NUM_15 };

static const int KEY_MAP[ROWS][COLS] = {
    { 1, 2, 3 },    /* Do, Re, Mi */
    { 4, 5, 6 },    /* Fa, Sol, La */
    { 7, 8, 9 },    /* Si, 高Do, 高Re */
};

/* 消抖状态 */
static bool    g_raw[ROWS][COLS];
static bool    g_last_raw[ROWS][COLS];
static bool    g_stable[ROWS][COLS];
static int64_t g_change_time[ROWS][COLS];

static int g_pressed_row = -1;
static int g_pressed_col = -1;

/* 扫描一次原始状态 */
static void scan_raw(void)
{
    /* 所有行先切高阻 + 上拉，避免上一轮残留低电平 */
    for (int r = 0; r < ROWS; r++) {
        gpio_set_direction(ROW_PINS[r], GPIO_MODE_INPUT);
        gpio_set_pull_mode(ROW_PINS[r], GPIO_PULLUP_ONLY);
    }
    esp_rom_delay_us(50);   /* 等所有行恢复到高电平 */

    /* 逐行拉低、读列 */
    for (int r = 0; r < ROWS; r++) {
        gpio_set_direction(ROW_PINS[r], GPIO_MODE_OUTPUT);
        gpio_set_level(ROW_PINS[r], 0);
        esp_rom_delay_us(100);  /* 拉低稳定 */

        for (int c = 0; c < COLS; c++) {
            g_raw[r][c] = (gpio_get_level(COL_PINS[c]) == 0);
        }

        /* 关键：恢复高阻时强制上拉。
         * 否则该行引脚残留 LOW 电平，会通过按下的按键把列线拉低，
         * 导致下一行扫描误判成当前行与同一列的交点。 */
        gpio_set_direction(ROW_PINS[r], GPIO_MODE_INPUT);
        gpio_set_pull_mode(ROW_PINS[r], GPIO_PULLUP_ONLY);
        esp_rom_delay_us(50);
    }
}

/* 消抖 + 更新稳定状态 */
static void update(void)
{
    scan_raw();
    int64_t now = esp_timer_get_time();

    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {

            if (g_raw[r][c] != g_last_raw[r][c]) {
                g_last_raw[r][c] = g_raw[r][c];
                g_change_time[r][c] = now;
            }

            if ((now - g_change_time[r][c]) >= DEBOUNCE_US &&
                g_stable[r][c] != g_raw[r][c]) {

                g_stable[r][c] = g_raw[r][c];

                if (g_stable[r][c]) {
                    g_pressed_row = r;
                    g_pressed_col = c;
                } else {
                    if (g_pressed_row == r && g_pressed_col == c) {
                        g_pressed_row = -1;
                        g_pressed_col = -1;
                    }
                }
            }
        }
    }
}

esp_err_t keypad_init(void)
{
    ESP_LOGI(TAG, "初始化 3x3 矩阵键盘 (行38-39-21, 列35-37)...");

    for (int r = 0; r < ROWS; r++) {
        gpio_reset_pin(ROW_PINS[r]);
        gpio_set_direction(ROW_PINS[r], GPIO_MODE_INPUT);  /* 初始高阻 */
        gpio_set_pull_mode(ROW_PINS[r], GPIO_PULLUP_ONLY);  /* 上拉防残留 */
    }
    for (int c = 0; c < COLS; c++) {
        gpio_reset_pin(COL_PINS[c]);
        gpio_set_direction(COL_PINS[c], GPIO_MODE_INPUT);
        gpio_set_pull_mode(COL_PINS[c], GPIO_PULLUP_ONLY);
    }

    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++)
            g_last_raw[r][c] = g_stable[r][c] = false;

    ESP_LOGI(TAG, "矩阵键盘初始化完成");
    return ESP_OK;
}

int keypad_get_key(void)
{
    update();
    if (g_pressed_row >= 0 && g_pressed_col >= 0)
        return KEY_MAP[g_pressed_row][g_pressed_col];
    return 0;
}

uint16_t keypad_get_pressed_mask(void)
{
    update();
    uint16_t mask = 0;
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++)
            if (g_stable[r][c])
                mask |= (1u << KEY_MAP[r][c]);
    return mask;
}

int keypad_get_row(void) { return g_pressed_row; }
int keypad_get_col(void) { return g_pressed_col; }
