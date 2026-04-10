#include "sensor_modules.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

static const char *TAG = "SENSORS";

// 全局变量保存传感器最新数据
static float current_heading = 0.0;
static double current_latitude = 0.0;
static double current_longitude = 0.0;
static bool has_gps_fix = false;

// ADC 句柄和校验信息
static adc_oneshot_unit_handle_t adc1_handle;
static adc_cali_handle_t adc1_cali_handle;
static bool do_calibration1;

// ==========================================
// 1. 地磁模块 I2C 初始化
// ==========================================
void compass_i2c_init(void) {
    ESP_LOGI(TAG, "初始化 I2C 总线 (地磁模块)");
    
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_SDA_PIN,
        .scl_io_num = I2C_SCL_PIN,
        // 硬件已经有 2.7K 外部上拉电阻，因此关闭内部上拉
        .sda_pullup_en = GPIO_PULLUP_DISABLE,
        .scl_pullup_en = GPIO_PULLUP_DISABLE,
        .master.clk_speed = I2C_FREQ_HZ,
    };
    
    ESP_ERROR_CHECK(i2c_param_config(I2C_MASTER_PORT, &conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_MASTER_PORT, conf.mode, 0, 0, 0));
    
    ESP_LOGI(TAG, "I2C 总线初始化完成");
    // 注：MMC5603NJ 的 I2C 器件地址通常为 0x30
}

// ==========================================
// 2. GPS 模块 UART & PPS 初始化
// ==========================================
void gps_uart_init(void) {
    ESP_LOGI(TAG, "初始化 UART 总线 (GPS 模块)");

    // 配置 UART 参数
    uart_config_t uart_config = {
        .baud_rate = GPS_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    
    // 安装 UART 驱动程序 (分配 2048 字节的接收缓冲区，不使用发送缓冲区)
    ESP_ERROR_CHECK(uart_driver_install(GPS_UART_PORT, 2048, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(GPS_UART_PORT, &uart_config));
    
    // 配置 UART 引脚 (注意：TX/RX 引脚的交叉映射逻辑)
    ESP_ERROR_CHECK(uart_set_pin(GPS_UART_PORT, ESP_TX_TO_GPS_RX_PIN, ESP_RX_FROM_GPS_TX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    ESP_LOGI(TAG, "初始化 PPS 引脚");
    // 配置 PPS 引脚为输入模式，如果你需要高精度授时，可以开启上升沿中断
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE, // 目前暂不开启中断，如需授时可改为 GPIO_INTR_POSEDGE
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << GPS_PPS_PIN),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    ESP_LOGI(TAG, "GPS UART & PPS 初始化完成");
}

// ==========================================
// 实时数据获取任务 (GPS 与 罗盘)
// ==========================================

// GPS NMEA 解析任务
static void gps_task(void *arg) {
    uint8_t data[128];
    char line_buf[128];
    int line_idx = 0;

    while (1) {
        // 从 UART 读取数据包
        int len = uart_read_bytes(GPS_UART_PORT, data, sizeof(data) - 1, pdMS_TO_TICKS(100));
        for (int i = 0; i < len; i++) {
            if (data[i] == '\n') {
                line_buf[line_idx] = '\0';
                
                // 过滤出 RMC 语句 (支持 GNRMC, GPRMC, BDRMC 等)
                if ((strncmp(line_buf, "$G", 2) == 0 || strncmp(line_buf, "$B", 2) == 0) && strstr(line_buf, "RMC")) {
                    char *p = line_buf;
                    char *token;
                    int field = 0;
                    char status = 'V';
                    double lat = 0, lon = 0;
                    char ns = 'N', ew = 'E';

                    // 使用 strsep 完美处理连续逗号导致的空字段问题
                    while ((token = strsep(&p, ",")) != NULL) {
                        if (field == 2 && token[0] != '\0') status = token[0];       // 状态 A=有效 V=无效
                        else if (field == 3 && token[0] != '\0') lat = atof(token);  // 纬度 (ddmm.mmmm)
                        else if (field == 4 && token[0] != '\0') ns = token[0];      // N/S
                        else if (field == 5 && token[0] != '\0') lon = atof(token);  // 经度 (dddmm.mmmm)
                        else if (field == 6 && token[0] != '\0') ew = token[0];      // E/W
                        field++;
                    }

                    // 如果定位有效，转换坐标系并存入全局变量
                    if (status == 'A') {
                        int lat_deg = (int)(lat / 100);
                        current_latitude = lat_deg + (lat - lat_deg * 100) / 60.0;
                        if (ns == 'S') current_latitude = -current_latitude;

                        int lon_deg = (int)(lon / 100);
                        current_longitude = lon_deg + (lon - lon_deg * 100) / 60.0;
                        if (ew == 'W') current_longitude = -current_longitude;

                        has_gps_fix = true;
                    } else {
                        has_gps_fix = false;
                    }
                }
                line_idx = 0;
            } else if (data[i] != '\r' && line_idx < sizeof(line_buf) - 1) {
                line_buf[line_idx++] = data[i];
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// 地磁数据轮询任务 (MMC5603NJ)
static void compass_task(void *arg) {
    uint8_t write_buf[2];
    uint8_t read_buf[6];
    
    // 发送一次 SET 指令 (寄存器 0x1B, 写入 0x08)，消除剩余磁化
    write_buf[0] = 0x1B; write_buf[1] = 0x08;
    i2c_master_write_to_device(I2C_MASTER_PORT, 0x30, write_buf, 2, pdMS_TO_TICKS(100));
    vTaskDelay(pdMS_TO_TICKS(10));

    while (1) {
        // 1. 发送触发测量指令 (寄存器 0x1B, 写入 0x01: TAKE_MEASUREMENT_M)
        write_buf[0] = 0x1B;
        write_buf[1] = 0x01;
        i2c_master_write_to_device(I2C_MASTER_PORT, 0x30, write_buf, 2, pdMS_TO_TICKS(100));
        
        // 2. 等待磁力计测量完成 (~10ms)
        vTaskDelay(pdMS_TO_TICKS(15));
        
        // 3. 读取 X, Y, Z 数据 (从寄存器 0x00 开始读 6 字节)
        write_buf[0] = 0x00;
        if (i2c_master_write_read_device(I2C_MASTER_PORT, 0x30, write_buf, 1, read_buf, 6, pdMS_TO_TICKS(100)) == ESP_OK) {
            
            // 4. 解析 16 位 X, Y, Z 数据 (MSB first)
            // MMC5603 X: [0]=MSB, [1]=LSB; Y: [2]=MSB, [3]=LSB; Z: [4]=MSB, [5]=LSB
            // 在 16bit 模式下，零磁场对应的值是 32768
            int32_t x = (read_buf[0] << 8) | read_buf[1];
            int32_t y = (read_buf[2] << 8) | read_buf[3];
            int32_t z = (read_buf[4] << 8) | read_buf[5];
            
            // 归零偏置
            x -= 32768; 
            y -= 32768;
            z -= 32768;
            
            // 此处后续可加入硬铁软铁校准 (CALI:START触发的最小最大值拟合补偿)
            // 目前先计算原始航向角 (0-360)
            // 注意: X, Y 轴与板子丝印的对应关系如果反了，可以在此处更换
            float heading = atan2((double)y, (double)x) * 180.0 / 3.14159265;
            if (heading < 0) heading += 360.0;
            
            current_heading = heading;
        }
        
        // 保持大约 50Hz 刷新率
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

// ==========================================
// 3. 电池电量 ADC 初始化
// ==========================================
static bool adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle) {
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI(TAG, "曲线拟合校准支持");
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = unit,
            .chan = channel,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

    *out_handle = handle;
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "ADC 校准初始化成功");
    } else {
        ESP_LOGI(TAG, "ADC 校准初始化失败");
    }
    return calibrated;
}

void battery_adc_init(void) {
    // IO14 对应 ESP32-S3 的 ADC1_CHANNEL_3 (如果不是可以用 adc_oneshot_io_to_channel 查)
    adc_channel_t channel = ADC_CHANNEL_3; 

    // 初始化 ADC1
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

    // 配置通道
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12, // ESP32-S3 通常使用 12dB 衰减来测最高约 3.1V
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, channel, &config));

    // 校准
    do_calibration1 = adc_calibration_init(ADC_UNIT_1, channel, ADC_ATTEN_DB_12, &adc1_cali_handle);
    ESP_LOGI(TAG, "电池 ADC 初始化完成");
}

float battery_get_voltage(void) {
    int raw_val = 0;
    int voltage_mv = 0;
    adc_channel_t channel = ADC_CHANNEL_3; 
    
    ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, channel, &raw_val));
    if (do_calibration1) {
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_handle, raw_val, &voltage_mv));
    } else {
        // 无校准的粗略估算
        voltage_mv = raw_val * 3100 / 4095;
    }
    
    // 注意：假设外围电路有分压电阻，这里需要根据原理图分压比换算为实际电池电压
    // 例如分压比是 1:2，则实际电压 = voltage_mv * 2
    // 这里先直接返回引脚电压（伏特）
    return (float)voltage_mv / 1000.0f;
}

// 对外接口
void sensors_start_tasks(void) {
    xTaskCreatePinnedToCore(gps_task, "gps_task", 4096, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(compass_task, "compass_task", 4096, NULL, 5, NULL, 1);
    ESP_LOGI(TAG, "传感器后台获取任务 (GPS & 罗盘) 已启动");
}

float compass_get_heading(void) {
    return current_heading;
}

bool gps_get_current_location(double *out_lat, double *out_lon) {
    if (has_gps_fix) {
        *out_lat = current_latitude;
        *out_lon = current_longitude;
        return true;
    }
    return false;
}