#pragma once

#include "driver/i2c.h"
#include "driver/uart.h"
#include "driver/gpio.h"

// ===== 地磁模块 (I2C) 配置 =====
#define I2C_MASTER_PORT I2C_NUM_0
#define I2C_SDA_PIN     1
#define I2C_SCL_PIN     2
#define I2C_FREQ_HZ     400000 // 400kHz 快速模式

// ===== GPS 模块 (UART) 配置 =====
#define GPS_UART_PORT   UART_NUM_1
#define GPS_BAUD_RATE   9600   // 中科微 GPS 默认通常为 9600
#define ESP_TX_TO_GPS_RX_PIN   8     // 对应原理图 GPS_TX 所在 IO (通常这是给GPS接收)
#define ESP_RX_FROM_GPS_TX_PIN 18    // 对应原理图 GPS_RX 所在 IO (通常这是从GPS发送)
#define GPS_PPS_PIN     17     // PPS 秒脉冲引脚

// ===== 电池电量 (ADC) 配置 =====
#define BAT_ADC_PIN     14     // 原理图 READ_POWER

// 函数声明
void compass_i2c_init(void);
void gps_uart_init(void);
void battery_adc_init(void);
float battery_get_voltage(void);

// 传感器后台任务与数据获取接口
void sensors_start_tasks(void);
float compass_get_heading(void);
bool gps_get_current_location(double *out_lat, double *out_lon);