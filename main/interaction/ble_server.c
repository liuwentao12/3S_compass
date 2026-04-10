#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_defs.h"
#include "esp_gatt_common_api.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_system.h"
#include <string.h>

#include "Navigation/navigator_logic.h"

static const char *TAG = "BLE_SERVER";

#define GATTS_SERVICE_UUID   0x00FF
#define GATTS_CHAR_UUID_CMD  0xFF01
#define GATTS_CHAR_UUID_OTA  0xFF02

// 在 main.c 中声明，用于更新坐标
extern void app_set_target(double lat, double lon);

static uint16_t gatts_if_for_app = ESP_GATT_IF_NONE;
static uint16_t char_handle_cmd = 0;
static uint16_t char_handle_ota = 0;

// OTA 相关状态
static esp_ota_handle_t update_handle = 0;
static const esp_partition_t *update_partition = NULL;
static bool ota_in_progress = false;

// 蓝牙广播参数
static esp_ble_adv_params_t adv_params = {
    .adv_int_min        = 0x20,
    .adv_int_max        = 0x40,
    .adv_type           = ADV_TYPE_IND,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy  = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    if (event == ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT) {
        esp_ble_gap_start_advertising(&adv_params);
        ESP_LOGI(TAG, "BLE 开始广播...");
    }
}

static void gatts_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
    switch (event) {
    case ESP_GATTS_REG_EVT: {
        esp_ble_gap_set_device_name("3S_Compass");
        
        esp_ble_adv_data_t adv_data = {
            .set_scan_rsp = false,
            .include_name = true,
            .min_interval = 0x0006,
            .max_interval = 0x0010,
            .appearance = 0x00,
            .manufacturer_len = 0,
            .p_manufacturer_data =  NULL,
            .service_data_len = 0,
            .p_service_data = NULL,
            .service_uuid_len = 0,
            .p_service_uuid = NULL,
            .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
        };
        esp_ble_gap_config_adv_data(&adv_data);

        esp_gatt_srvc_id_t service_id;
        service_id.is_primary = true;
        service_id.id.inst_id = 0x00;
        service_id.id.uuid.len = ESP_UUID_LEN_16;
        service_id.id.uuid.uuid.uuid16 = GATTS_SERVICE_UUID;
        esp_ble_gatts_create_service(gatts_if, &service_id, 8); // 增加 handles 数量以支持多个特征
        break;
    }
    case ESP_GATTS_CREATE_EVT: {
        esp_bt_uuid_t char_uuid_cmd;
        char_uuid_cmd.len = ESP_UUID_LEN_16;
        char_uuid_cmd.uuid.uuid16 = GATTS_CHAR_UUID_CMD;
        esp_ble_gatts_add_char(param->create.service_handle, &char_uuid_cmd,
                               ESP_GATT_PERM_WRITE,
                               ESP_GATT_CHAR_PROP_BIT_WRITE,
                               NULL, NULL);
                               
        esp_bt_uuid_t char_uuid_ota;
        char_uuid_ota.len = ESP_UUID_LEN_16;
        char_uuid_ota.uuid.uuid16 = GATTS_CHAR_UUID_OTA;
        esp_ble_gatts_add_char(param->create.service_handle, &char_uuid_ota,
                               ESP_GATT_PERM_WRITE,
                               ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_WRITE_NR,
                               NULL, NULL);
        
        esp_ble_gatts_start_service(param->create.service_handle);
        break;
    }
    case ESP_GATTS_ADD_CHAR_EVT: {
        if (param->add_char.char_uuid.uuid.uuid16 == GATTS_CHAR_UUID_CMD) {
            char_handle_cmd = param->add_char.attr_handle;
        } else if (param->add_char.char_uuid.uuid.uuid16 == GATTS_CHAR_UUID_OTA) {
            char_handle_ota = param->add_char.attr_handle;
        }
        break;
    }
    case ESP_GATTS_WRITE_EVT: {
        if (param->write.handle == char_handle_cmd) {
            char cmd[128];
            int len = param->write.len < 127 ? param->write.len : 127;
            memcpy(cmd, param->write.value, len);
            cmd[len] = '\0';
            ESP_LOGI(TAG, "📱 收到手机蓝牙指令: %s", cmd);

            if (strncmp(cmd, "DEST:", 5) == 0) {
                double t_lat = 0, t_lon = 0;
                if (sscanf(cmd + 5, "%lf,%lf", &t_lat, &t_lon) == 2) {
                    app_set_target(t_lat, t_lon);
                    set_navigation_mode(MODE_NAVIGATION);
                }
            } else if (strncmp(cmd, "MODE:COMPASS", 12) == 0) {
                set_navigation_mode(MODE_COMPASS);
            } else if (strncmp(cmd, "CALI:START", 10) == 0) {
                ESP_LOGI(TAG, "🔄 触发蓝牙罗盘画8字校准");
            } else if (strncmp(cmd, "OTA:START", 9) == 0) {
                ESP_LOGI(TAG, "🚀 开始蓝牙 OTA 更新...");
                update_partition = esp_ota_get_next_update_partition(NULL);
                if (update_partition != NULL) {
                    esp_err_t err = esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &update_handle);
                    if (err == ESP_OK) {
                        ota_in_progress = true;
                        ESP_LOGI(TAG, "OTA 初始化成功，准备接收数据");
                    } else {
                        ESP_LOGE(TAG, "OTA 初始化失败! err=0x%x", err);
                    }
                }
            } else if (strncmp(cmd, "OTA:END", 7) == 0) {
                if (ota_in_progress) {
                    esp_err_t err = esp_ota_end(update_handle);
                    if (err == ESP_OK) {
                        err = esp_ota_set_boot_partition(update_partition);
                        if (err == ESP_OK) {
                            ESP_LOGI(TAG, "✅ OTA 更新完成，即将重启系统...");
                            vTaskDelay(1000 / portTICK_PERIOD_MS);
                            esp_restart();
                        } else {
                            ESP_LOGE(TAG, "OTA 设置启动分区失败! err=0x%x", err);
                        }
                    } else {
                        ESP_LOGE(TAG, "OTA 结束失败! err=0x%x", err);
                    }
                    ota_in_progress = false;
                }
            }
            
            if (param->write.need_rsp) {
                esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
            }
        } else if (param->write.handle == char_handle_ota) {
            // 处理 OTA 数据块
            if (ota_in_progress && param->write.len > 0) {
                esp_err_t err = esp_ota_write(update_handle, param->write.value, param->write.len);
                if (err != ESP_OK) {
                    ESP_LOGE(TAG, "OTA 写入数据失败! err=0x%x", err);
                    ota_in_progress = false;
                }
            }
            if (param->write.need_rsp) {
                esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
            }
        }
        break;
    }
    default:
        break;
    }
}

static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
    if (event == ESP_GATTS_REG_EVT) gatts_if_for_app = gatts_if;
    if (gatts_if == ESP_GATT_IF_NONE || gatts_if == gatts_if_for_app) {
        gatts_profile_event_handler(event, gatts_if, param);
    }
}

void ble_server_init(void) {
    ESP_LOGI(TAG, "初始化低功耗蓝牙 (BLE)...");
    
    // ESP32-S3 必须释放经典蓝牙内存
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));
    
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));
    
    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());
    
    ESP_ERROR_CHECK(esp_ble_gatts_register_callback(gatts_event_handler));
    ESP_ERROR_CHECK(esp_ble_gap_register_callback(gap_event_handler));
    ESP_ERROR_CHECK(esp_ble_gatts_app_register(0));
}
