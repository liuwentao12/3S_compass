#include "voice_assistant.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// ESP-SR 头文件
#include "esp_process_sdkconfig.h"
#include "model_path.h"
#include "esp_afe_sr_models.h"
#include "esp_mn_models.h"

static const char *TAG = "VOICE_ASSISTANT";

static void feed_audio_task(void *arg) {
    ESP_LOGI(TAG, "语音采样喂狗任务启动...");
    // 实际项目中，你需要从 I2S 读取麦克风数据
    // 并调用 afe_handle->feed() 塞入音频流
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(100)); // 占位
    }
}

static void detect_audio_task(void *arg) {
    ESP_LOGI(TAG, "语音唤醒识别任务启动...");
    // 实际项目中，调用 afe_handle->fetch() 获取处理后的音频
    // 传入 wakenet 进行关键词识别
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(100)); // 占位
    }
}

void voice_assistant_init(void) {
    ESP_LOGI(TAG, "正在初始化 ESP-SR 语音助手...");
    
    // 注意：ESP-SR 的完整模型加载需要 SPI RAM (PSRAM) 以及模型分区
    // 并且需要在 menuconfig 中配置使用的模型 (如 wn9_hixin 用于 "Hi 乐鑫")
    // 这里我们先搭建任务框架，等待硬件测试和 PSRAM 配置完毕后填充具体 API

    xTaskCreatePinnedToCore(feed_audio_task, "feed_audio", 4096, NULL, 5, NULL, 0);
    xTaskCreatePinnedToCore(detect_audio_task, "detect_audio", 8192, NULL, 4, NULL, 1);
    
    ESP_LOGI(TAG, "语音助手基础框架已建立。唤醒词：[Hi Lexin / Hi 乐鑫]");
}
