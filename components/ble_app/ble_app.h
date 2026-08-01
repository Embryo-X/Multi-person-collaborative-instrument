/**
 * ==============================================================
 * BLE 蓝牙组件 — GATT Server + 广播（Embryo Conductor 协议）
 * ==============================================================
 * 设备名: Embryo-Perc
 * Service: 6e400001-b5a3-f393-e0a9-e50e24dcca9e
 * 特征: Role / SongData / Control / Status / Meta
 * ==============================================================
 */

#ifndef BLE_APP_H
#define BLE_APP_H

#include "esp_err.h"

esp_err_t ble_app_init(void);          /* 初始化 BLE + 开始广播 */
int ble_app_is_connected(void);        /* 返回 1=有设备连接 */
uint8_t ble_app_get_role(void);        /* 返回角色 0x01=打击 0x02=弦乐 0=未分配 */
void ble_app_deinit(void);             /* 停广播 + 断开 */

#endif
