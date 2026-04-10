#pragma once

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"

// TFT 屏幕引脚定义 (基于您的原理图)
#define TFT_SPI_HOST    SPI2_HOST
#define TFT_SCK_PIN     42
#define TFT_MOSI_PIN    41
#define TFT_MISO_PIN    -1  
#define TFT_DC_PIN      40
#define TFT_CS_PIN      39
#define TFT_BLK_PIN     38
#define TFT_RST_PIN     -1  // 硬件复位已接 EN 引脚，故设为 -1

// 屏幕分辨率 (请根据您的实际屏幕修改)
#define TFT_H_RES       240
#define TFT_V_RES       240

// 初始化函数声明
esp_lcd_panel_handle_t tft_display_init(void);