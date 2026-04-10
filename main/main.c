#include "interaction/audio.h"
#include "interaction/voice_assistant.h"
#include "interaction/tft_display.h"
#include "mini_claw/mini_config.h"
#include "Navigation/sensor_modules.h"
#include "Navigation/navigator_logic.h"
#include "storage/sd_card.h"
#include "interaction/ble_server.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "MAIN";

// 全局目标坐标
static double target_lat = 39.916345;
static double target_lon = 116.397155;

// 提供给外界(串口或蓝牙)修改目标的接口
void app_set_target(double lat, double lon) {
    target_lat = lat;
    target_lon = lon;
    ESP_LOGI(TAG, "✈️ 收到指令，设定新目标: Lat=%.6f, Lon=%.6f", target_lat, target_lon);
}

// 上位机串口指令监听任务
static void serial_cmd_task(void *arg) {
    char line[128];
    while (1) {
        // 从标准输入(USB串口)读取一行
        if (fgets(line, sizeof(line), stdin) != NULL) {
            // 清除可能带入的换行符
            line[strcspn(line, "\r\n")] = 0;
            
            // 解析目标设定指令，例如: DEST:39.916345,116.397155
            if (strncmp(line, "DEST:", 5) == 0) {
                double t_lat = 0, t_lon = 0;
                if (sscanf(line + 5, "%lf,%lf", &t_lat, &t_lon) == 2) {
                    app_set_target(t_lat, t_lon);
                    set_navigation_mode(MODE_NAVIGATION);
                }
            }
            // 解析校准指令，例如: CALI:START
            else if (strncmp(line, "CALI:START", 10) == 0) {
                ESP_LOGI(TAG, "🔄 收到校准指令，请手持设备在空中画 '8' 字...");
                // TODO: 此处可触发传感器校准模式的回调，收集极大极小值计算硬铁偏移
            }
            else if (strncmp(line, "MODE:COMPASS", 12) == 0) {
                set_navigation_mode(MODE_COMPASS);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "=== 3S_Compass 系统启动 ===");

    // NVS 初始化 (蓝牙和WiFi必须依赖它)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // 初始化各个子系统
    audio_i2s_init();
    tft_display_init();
    compass_i2c_init();
    gps_uart_init();
    indicator_leds_init();
    battery_adc_init();
    voice_assistant_init();
    ble_server_init(); // 启动蓝牙服务

    // 初始化 SD 卡并挂载文件系统
    if (sd_card_init() == ESP_OK) {
        ESP_LOGI(TAG, "SD 卡挂载成功！可以读取离线资源。");
    } else {
        ESP_LOGW(TAG, "SD 卡未检测到或挂载失败，将跳过部分依赖功能。");
    }

    // 默认是指南针模式
    set_navigation_mode(MODE_COMPASS);

    // 启动传感器后台读数任务
    sensors_start_tasks();

    // 启动串口监听任务以接收上位机指令
    xTaskCreatePinnedToCore(serial_cmd_task, "serial_cmd_task", 4096, NULL, 5, NULL, 0);

    while (1)
    {
        // 1. 获取当前最新硬件数据
        float heading = compass_get_heading();
        double cur_lat = 0, cur_lon = 0;
        bool has_gps = gps_get_current_location(&cur_lat, &cur_lon);

        if (has_gps) {
            ESP_LOGI(TAG, "[已定位] 经度:%.5f 纬度:%.5f | 罗盘朝向:%.1f°", cur_lon, cur_lat, heading);
        } else {
            ESP_LOGI(TAG, "[GPS 搜星中...] | 罗盘朝向:%.1f°", heading);
        }

        // 2. 核心更新：将真实传感器数据传入导航引擎并刷新灯环
        update_navigation_task(heading, cur_lat, cur_lon, target_lat, target_lon);

        // 3. 定期打印电量
        static int tick = 0;
        tick++;
        if (tick % 5 == 0) {
            float vbat = battery_get_voltage();
            // 如果有分压电阻（如10k/10k分压），则实际电池电压为 vbat * 2
            ESP_LOGI(TAG, "[状态监测] ADC 引脚电压: %.2f V, 估算电池电压: %.2f V", vbat, vbat * 2.0);
        }

        vTaskDelay(pdMS_TO_TICKS(1000)); // 每秒刷新一次
    }
}
