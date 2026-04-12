#pragma once

#include "esp_err.h"
#include <stdbool.h>

/**
 * @brief 初始化 GPS 轨迹记录器（在 SD 卡挂载后调用）
 * 
 * @return esp_err_t ESP_OK 成功，ESP_FAIL 失败
 */
esp_err_t track_logger_init(void);

/**
 * @brief 记录一条 GPS 轨迹数据到 SD 卡
 * 
 * @param lat 纬度
 * @param lon 经度
 * @param heading 航向（罗盘朝向）
 * @return esp_err_t 
 */
esp_err_t track_logger_record(double lat, double lon, float heading);
