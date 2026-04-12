#include "track_logger.h"
#include "esp_log.h"
#include <stdio.h>
#include <sys/time.h>

static const char *TAG = "TRACK_LOGGER";
static const char *FILE_PATH = "/sdcard/track.csv";
static bool is_initialized = false;

esp_err_t track_logger_init(void) {
    // 尝试以追加模式打开文件
    FILE *f = fopen(FILE_PATH, "a");
    if (f == NULL) {
        ESP_LOGE(TAG, "无法创建或打开轨迹文件 %s", FILE_PATH);
        return ESP_FAIL;
    }
    
    // 检查文件大小，如果为空则写入 CSV 表头
    fseek(f, 0, SEEK_END);
    if (ftell(f) == 0) {
        fprintf(f, "Timestamp_ms,Latitude,Longitude,Heading\n");
    }
    fclose(f);
    
    is_initialized = true;
    ESP_LOGI(TAG, "GPS 轨迹记录器初始化完成，保存路径：%s", FILE_PATH);
    return ESP_OK;
}

esp_err_t track_logger_record(double lat, double lon, float heading) {
    if (!is_initialized) return ESP_FAIL;

    // 每次以追加方式打开文件并写入，完成后立即关闭以确保掉电数据不丢失
    FILE *f = fopen(FILE_PATH, "a");
    if (f == NULL) {
        ESP_LOGW(TAG, "无法打开轨迹文件进行追加写入");
        return ESP_FAIL;
    }

    struct timeval tv;
    gettimeofday(&tv, NULL);
    long long timestamp_ms = (long long)tv.tv_sec * 1000 + (tv.tv_usec / 1000);

    fprintf(f, "%lld,%.6f,%.6f,%.1f\n", timestamp_ms, lat, lon, heading);
    fclose(f);
    
    ESP_LOGI(TAG, "✅ 轨迹点已保存到 SD 卡 -> lat:%.5f, lon:%.5f", lat, lon);
    return ESP_OK;
}
