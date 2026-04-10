#pragma once

#include "esp_err.h"

// SD 卡引脚定义 (基于 SDMMC 1-line 模式)
#define SD_DAT0_PIN 21
#define SD_CLK_PIN  47
#define SD_CMD_PIN  48

// 初始化 SD 卡并挂载 FATFS
esp_err_t sd_card_init(void);

// 卸载 SD 卡
void sd_card_deinit(void);
