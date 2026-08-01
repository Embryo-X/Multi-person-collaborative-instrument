/**
 * ==============================================================
 * Wi-Fi 联网组件实现 — 占位模块
 * ==============================================================
 * TODO: 实现 Wi-Fi STA 连接逻辑
 *
 * 参考 ESP-IDF 示例：
 *   examples/wifi/getting_started/station/main/station_example_main.c
 *
 * 大致实现步骤：
 *   1. nvs_flash_init()            ← 初始化 NVS（存储 Wi-Fi 凭据）
 *   2. esp_netif_init()            ← 初始化 TCP/IP 栈
 *   3. esp_event_loop_create()     ← 创建事件循环
 *   4. esp_netif_create_default_wifi_sta()  ← 创建 Wi-Fi STA 接口
 *   5. esp_wifi_init()             ← 初始化 Wi-Fi 驱动
 *   6. esp_wifi_set_mode(WIFI_MODE_STA)     ← 设置为 STA 模式
 *   7. esp_wifi_set_config()       ← 设置 SSID 和密码
 *   8. esp_wifi_start()            ← 启动 Wi-Fi
 *   9. esp_wifi_connect()          ← 发起连接
 * ==============================================================
 */

#include "wifi_app.h"
#include "esp_log.h"

static const char *TAG = "wifi_app";

esp_err_t wifi_app_init(void) {
    ESP_LOGW(TAG, "Wi-Fi 组件尚未实现，跳过初始化");
    // TODO: 在此实现 Wi-Fi 初始化
    return ESP_OK;
}
