#include "sensor_modules.h"
#include "esp_log.h"

static const char *TAG = "SENSORS";

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
    ESP_ERROR_CHECK(uart_set_pin(GPS_UART_PORT, GPS_TX_PIN, GPS_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

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