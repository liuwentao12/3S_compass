#pragma once

#include "driver/i2c.h"
#include "driver/uart.h"
#include "driver/gpio.h"

// ===== 地磁模块 (I2C) 配置 =====
#define I2C_MASTER_PORT I2C_NUM_0
#define I2C_SDA_PIN     39
#define I2C_SCL_PIN     38
#define I2C_FREQ_HZ     400000 // 400kHz 快速模式

// ===== GPS 模块 (UART) 配置 =====
#define GPS_UART_PORT   UART_NUM_1
#define GPS_BAUD_RATE   9600   // 中科微 GPS 默认通常为 9600
#define GPS_TX_PIN      11     // 连接到模块的 RX
#define GPS_RX_PIN      12     // 连接到模块的 TX
#define GPS_PPS_PIN     10     // PPS 秒脉冲引脚

// 函数声明
void compass_i2c_init(void);
void gps_uart_init(void);