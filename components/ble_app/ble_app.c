/**
 * ==============================================================
 * BLE 蓝牙 — ESP-IDF v5.5 Bluedroid GATT Server
 * 对齐 ble-protocol.md v0.1：Embryo Conductor Service
 * ==============================================================
 * 设备名   : Embryo-Perc（打击）
 * Service  : 6e400001-b5a3-f393-e0a9-e50e24dcca9e (128-bit)
 * 特征      : Role(Write/Read) SongData(Write) Control(Write)
 *             Status(Notify/Read) Meta(Read)
 * 字节序   : 多字节一律小端
 * ==============================================================
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gatt_common_api.h"

#include "ble_app.h"
#include "song_guide.h"
#include "audio.h"
#include "led_bar.h"

static const char *TAG = "ble";

/* ---- 广播名 ---- */
#define DEVICE_NAME         "Embryo-Perc"

/* ---- 128-bit UUID 基（字符串: 6e4000XX-b5a3-f393-e0a9-e50e24dcca9e）
 * BLE 传输小端，数组为字符串反转 */
#define U128_BASE16    0x9e,0xca,0xdc,0x24,0x0e,0xe5,0xa9,0xe0,0x93,0xf3,0xa3,0xb5

#define UUID_SERVICE    { U128_BASE16, 0x01,0x00,0x40,0x6e }
#define UUID_ROLE       { U128_BASE16, 0x02,0x00,0x40,0x6e }
#define UUID_SONGDATA   { U128_BASE16, 0x03,0x00,0x40,0x6e }
#define UUID_CONTROL    { U128_BASE16, 0x04,0x00,0x40,0x6e }
#define UUID_STATUS     { U128_BASE16, 0x05,0x00,0x40,0x6e }
#define UUID_META       { U128_BASE16, 0x06,0x00,0x40,0x6e }

#define APP_ID          0
#define MAX_CHAR_NUM    5
#define MAX_HANDLE      12   /* 5特征×2 + 服务 */

/* ---- 状态 ---- */
static int g_connected = 0;
static int g_initialized = 0;
static esp_gatt_if_t g_gatts_if = 0;
static uint16_t g_conn_id = 0;
static uint16_t g_svc_handle = 0;
static uint16_t g_char_handle[MAX_CHAR_NUM] = {0};
static int  g_char_added = 0;   /* 已添加特征数 */
static uint16_t g_trans_id = 0;

/* 角色: 0x01=打击 0x02=弦乐 0x00=未分配 */
static uint8_t g_role = 0x01;

/* ---- 特征定义表 ---- */
typedef struct {
    const char *name;
    uint8_t uuid128[16];
    esp_gatt_perm_t perm;
    esp_gatt_char_prop_t prop;
} char_def_t;

static const char *CHAR_NAMES[MAX_CHAR_NUM] = { "Role", "SongData", "Control", "Status", "Meta" };

static const uint8_t CHAR_UUIDS[MAX_CHAR_NUM][16] = {
    UUID_ROLE,
    UUID_SONGDATA,
    UUID_CONTROL,
    UUID_STATUS,
    UUID_META,
};
static const esp_gatt_perm_t CHAR_PERMS[MAX_CHAR_NUM] = {
    ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,   /* Role */
    ESP_GATT_PERM_WRITE,                        /* SongData */
    ESP_GATT_PERM_WRITE,                        /* Control */
    ESP_GATT_PERM_READ,                         /* Status */
    ESP_GATT_PERM_READ,                         /* Meta */
};
static const esp_gatt_char_prop_t CHAR_PROPS[MAX_CHAR_NUM] = {
    ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE,  /* Role */
    ESP_GATT_CHAR_PROP_BIT_WRITE,                                 /* SongData */
    ESP_GATT_CHAR_PROP_BIT_WRITE,                                 /* Control */
    ESP_GATT_CHAR_PROP_BIT_NOTIFY | ESP_GATT_CHAR_PROP_BIT_READ,  /* Status */
    ESP_GATT_CHAR_PROP_BIT_READ,                                  /* Meta */
};

/* 特征句柄序号 */
enum { IDX_ROLE, IDX_SONGDATA, IDX_CONTROL, IDX_STATUS, IDX_META };

/* ---- Status 帧（固定 12 字节, 协议 §6, 全小端）---- */
static uint8_t g_status[12];
/* ---- Meta 帧（协议 §7）---- */
static const uint8_t g_meta[14] = {
    1, 0, 0,          /* fwMajor, fwMinor, fwPatch */
    10,               /* keyCount */
    0,                /* hasRail */
    1,                /* hasOled */
    'e','m','b','r','y','o','1','\0',  /* buildId[8] */
};

/* ---- 设备状态: 0=STANDBY 1=LOADED 2=PLAYING（hardcode 曲目上电即 LOADED）---- */
static uint8_t g_dev_state = 1;

/* ---- 构造 Status 帧：自动反映播放状态 ---- */
static void build_status(void)
{
    memset(g_status, 0, sizeof(g_status));
    if (song_guide_is_playing()) {
        g_dev_state = 2; /* PLAYING */
    } else if (g_dev_state == 2) {
        g_dev_state = 1; /* Believer 播完自动回 LOADED */
    }
    g_status[0] = g_dev_state;   /* state */
}

/* ---- 启动广播 ---- */
static void start_advertising(void)
{
    esp_ble_adv_params_t adv_params = {
        .adv_int_min        = 0x20,
        .adv_int_max        = 0x40,
        .adv_type           = ADV_TYPE_IND,
        .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
        .channel_map        = ADV_CHNL_ALL,
        .adv_filter_policy  = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
    };
    esp_ble_gap_start_advertising(&adv_params);
}

/* ---- 发送 Status Notify ---- */
static void notify_status(void)
{
    if (!g_connected || !g_char_handle[IDX_STATUS]) return;
    esp_ble_gatts_send_indicate(g_gatts_if, g_conn_id,
                                g_char_handle[IDX_STATUS],
                                sizeof(g_status), g_status, false);
}

/* ---- 添加下一个特征；全部加完则启动服务 ---- */
static void add_next_char(void)
{
    if (g_char_added >= MAX_CHAR_NUM) return;

    int idx = g_char_added;
    esp_bt_uuid_t u = { .len = ESP_UUID_LEN_128 };
    memcpy(u.uuid.uuid128, CHAR_UUIDS[idx], 16);

    esp_err_t ret = esp_ble_gatts_add_char(g_svc_handle, &u, CHAR_PERMS[idx], CHAR_PROPS[idx], NULL, NULL);
    if (ret != ESP_OK)
        ESP_LOGE(TAG, "add_char %s 失败: 0x%x", CHAR_NAMES[idx], ret);
    /* 剩余特征在 ADD_CHAR_EVT 中继续添加 */
}

/* ---- GATT 事件 ---- */
static void gatts_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if,
                          esp_ble_gatts_cb_param_t *param)
{
    switch (event) {

    case ESP_GATTS_REG_EVT: {
        g_gatts_if = gatts_if;
        g_trans_id = 0;
        g_char_added = 0;
        ESP_LOGI(TAG, "GATT 注册成功，创建服务...");
        esp_gatt_srvc_id_t srvc;
        memset(&srvc, 0, sizeof(srvc));
        srvc.is_primary = true;
        srvc.id.inst_id = 0;
        srvc.id.uuid.len = ESP_UUID_LEN_128;
        uint8_t svc_uuid[16] = UUID_SERVICE;
        memcpy(srvc.id.uuid.uuid.uuid128, svc_uuid, 16);
        esp_ble_gatts_create_service(gatts_if, &srvc, MAX_HANDLE);
        break;
    }

    case ESP_GATTS_CREATE_EVT:
        g_svc_handle = param->create.service_handle;
        ESP_LOGI(TAG, "服务已创建 (handle=%d)，添加特征...", g_svc_handle);
        g_char_added = 0;
        add_next_char();
        break;

    case ESP_GATTS_ADD_CHAR_EVT: {
        uint16_t h = param->add_char.attr_handle;
        if (h == 0) break;
        /* 按 uuid128[12] 定位特征（服务0x01, Role0x02...Meta0x06） */
        uint8_t tag = param->add_char.char_uuid.uuid.uuid128[12];
        int idx = tag - 0x02;
        if (idx >= 0 && idx < MAX_CHAR_NUM) {
            g_char_handle[idx] = h;
            ESP_LOGI(TAG, "特征 %s 已添加 (handle=%d)", CHAR_NAMES[idx], h);
        }
        g_char_added++;
        if (g_char_added < MAX_CHAR_NUM) {
            add_next_char();
        } else {
            ESP_LOGI(TAG, "5 个特征全部就绪，启动服务...");
            esp_ble_gatts_start_service(g_svc_handle);
        }
        break;
    }

    case ESP_GATTS_START_EVT:
        ESP_LOGI(TAG, "GATT Service 已启动，开始广播");
        start_advertising();
        break;

    case ESP_GATTS_CONNECT_EVT:
        g_connected = 1;
        g_conn_id = param->connect.conn_id;
        ESP_LOGI(TAG, ">>> 设备已连接 conn_id=%d <<<", g_conn_id);
        /* M2: 连接后 Notify 一帧 STANDBY */
        build_status();
        notify_status();
        break;

    case ESP_GATTS_DISCONNECT_EVT:
        g_connected = 0;
        g_conn_id = 0;
        ESP_LOGI(TAG, "<<< 设备已断开，停止所有操作 >>>");
        /* 断连后自动停止一切：时间轴+灯+声音 */
        song_guide_stop();   /* 内部会 led_bar_clear() 清灯 */
        audio_stop();        /* 停止所有 voice */
        led_bar_clear();     /* 双保险清灯 */
        g_dev_state = 1;     /* LOADED */
        start_advertising();
        break;

    case ESP_GATTS_READ_EVT: {
        if (!param->read.need_rsp) break;
        esp_gatt_rsp_t rsp;
        memset(&rsp, 0, sizeof(rsp));
        rsp.attr_value.handle = param->read.handle;

        if (param->read.handle == g_char_handle[IDX_ROLE]) {
            rsp.attr_value.len = 1;
            rsp.attr_value.value[0] = g_role;
        } else if (param->read.handle == g_char_handle[IDX_STATUS]) {
            build_status();
            rsp.attr_value.len = sizeof(g_status);
            memcpy(rsp.attr_value.value, g_status, sizeof(g_status));
        } else if (param->read.handle == g_char_handle[IDX_META]) {
            rsp.attr_value.len = sizeof(g_meta);
            memcpy(rsp.attr_value.value, g_meta, sizeof(g_meta));
        } else {
            esp_ble_gatts_send_response(gatts_if, param->read.conn_id,
                                        param->read.trans_id, ESP_GATT_REQ_NOT_SUPPORTED, &rsp);
            break;
        }
        esp_ble_gatts_send_response(gatts_if, param->read.conn_id,
                                    param->read.trans_id, ESP_GATT_OK, &rsp);
        break;
    }

    case ESP_GATTS_WRITE_EVT: {
        uint16_t h = param->write.handle;
        uint8_t *v = param->write.value;
        uint16_t len = param->write.len;

        if (h == g_char_handle[IDX_ROLE]) {
            if (len >= 1 && (v[0] == 0x00 || v[0] == 0x01 || v[0] == 0x02)) {
                g_role = v[0];
                ESP_LOGI(TAG, "Role → 0x%02X (%s)", g_role,
                         g_role == 0x01 ? "打击" : g_role == 0x02 ? "弦乐" : "未分配");
            }
        } else if (h == g_char_handle[IDX_CONTROL]) {
            uint8_t op = (len >= 1) ? v[0] : 0xFF;
            /* 蓝牙接收信号返回终端 */
            ESP_LOGI(TAG, "[BLE] Control 收到: opcode=0x%02X len=%u, 原始字节: %02X %02X %02X %02X %02X",
                     op, len, v[0], len>1?v[1]:0, len>2?v[2]:0, len>3?v[3]:0, len>4?v[4]:0);
            if (op == 0x01 && len >= 5) {
                /* START + uint32 delayMs LE */
                uint32_t delay = (uint32_t)v[1] | ((uint32_t)v[2] << 8) |
                                 ((uint32_t)v[3] << 16) | ((uint32_t)v[4] << 24);
                ESP_LOGI(TAG, "[BLE] → START delayMs=%u，启动 Believer 时间轴", (unsigned)delay);
                song_guide_start(delay);
                g_dev_state = 2;   /* PLAYING */
                build_status();
                notify_status();
            } else if (op == 0x02) {
                ESP_LOGI(TAG, "[BLE] → STOP，停止时间轴");
                song_guide_stop();
                g_dev_state = 1;   /* LOADED */
                build_status();
                notify_status();
            } else {
                ESP_LOGI(TAG, "[BLE] 未知 opcode=0x%02X (忽略)", op);
            }
        } else if (h == g_char_handle[IDX_SONGDATA]) {
            ESP_LOGI(TAG, "[BLE] SongData 收到 %u 字节 (hardcode 路径忽略)", len);
        } else {
            ESP_LOGW(TAG, "[BLE] 写入未知特征 handle=%d", h);
        }

        /* 回写响应：Web 用 Write With Response 时需要，否则 Central 超时报错 */
        if (param->write.need_rsp) {
            esp_gatt_rsp_t rsp;
            memset(&rsp, 0, sizeof(rsp));
            rsp.attr_value.handle = param->write.handle;
            esp_ble_gatts_send_response(gatts_if, param->write.conn_id,
                                        param->write.trans_id, ESP_GATT_OK, &rsp);
        }
        break;
    }

    case ESP_GATTS_MTU_EVT:
        ESP_LOGI(TAG, "MTU 协商: %d", param->mtu.mtu);
        break;

    default:
        break;
    }
}

/* ---- GAP 广播事件 ---- */
static void gap_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
        ESP_LOGI(TAG, "广播数据已设置");
        start_advertising();
        break;
    default:
        break;
    }
}

/* ---- 公开 API ---- */
esp_err_t ble_app_init(void)
{
    if (g_initialized) return ESP_OK;

    ESP_LOGI(TAG, "初始化 BLE (Embryo-Perc)...");

    esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_bt_controller_init(&bt_cfg);
    esp_bt_controller_enable(ESP_BT_MODE_BLE);
    esp_bluedroid_init();
    esp_bluedroid_enable();

    esp_ble_gatts_register_callback(gatts_handler);
    esp_ble_gap_register_callback(gap_handler);
    esp_ble_gatts_app_register(APP_ID);

    /* 设备名 */
    esp_ble_gap_set_device_name(DEVICE_NAME);

    /* 请求 MTU ≥ 185（协议要求） */
    esp_ble_gatt_set_local_mtu(500);

    /* 广播：名称 + 128-bit Service UUID */
    static uint8_t svc_adv[16] = UUID_SERVICE;
    esp_ble_adv_data_t adv = {
        .set_scan_rsp      = false,
        .include_name      = true,
        .include_txpower   = false,
        .service_uuid_len  = 16,
        .p_service_uuid    = svc_adv,
        .flag              = ESP_BLE_ADV_FLAG_GEN_DISC,
    };
    esp_ble_gap_config_adv_data(&adv);

    g_initialized = 1;
    ESP_LOGI(TAG, "BLE 初始化完成，设备名: %s", DEVICE_NAME);
    return ESP_OK;
}

int ble_app_is_connected(void) { return g_connected; }
uint8_t ble_app_get_role(void) { return g_role; }

void ble_app_deinit(void)
{
    if (g_connected) esp_ble_gatts_close(g_gatts_if, g_conn_id);
    esp_ble_gap_stop_advertising();
    g_connected = 0;
}
