#include "tft_display.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"

static const char *TAG = "TFT_DISPLAY";

esp_lcd_panel_handle_t tft_display_init(void) {
    ESP_LOGI(TAG, "初始化 SPI 总线");
    spi_bus_config_t buscfg = {
        .sclk_io_num = TFT_SCK_PIN,
        .mosi_io_num = TFT_MOSI_PIN,
        .miso_io_num = TFT_MISO_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = TFT_H_RES * 80 * sizeof(uint16_t), // 传输缓冲区大小(按需调整)
    };
    // 初始化 SPI 总线
    ESP_ERROR_CHECK(spi_bus_initialize(TFT_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO));

    ESP_LOGI(TAG, "分配 LCD IO 句柄");
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = TFT_DC_PIN,
        .cs_gpio_num = TFT_CS_PIN,
        .pclk_hz = 40 * 1000 * 1000, // SPI 时钟频率 (40MHz 一般屏幕都支持)
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,               // ST7789 通常使用 SPI Mode 0
        .trans_queue_depth = 10,
    };
    // 将 LCD 附加到 SPI 总线
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)TFT_SPI_HOST, &io_config, &io_handle));

    ESP_LOGI(TAG, "初始化 LCD 驱动面板");
    esp_lcd_panel_handle_t panel_handle = NULL;
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = TFT_RST_PIN, // 为 -1，不使用软件控制复位
        .rgb_endian = LCD_RGB_ENDIAN_RGB, // 颜色字节序 (若屏幕颜色反了，改成 BGR)
        .bits_per_pixel = 16,             // RGB565 标准 16bit 色彩
    };
    
    // 【重要提示】: 这里假设是 ST7789。如果是 ILI9341，请改为 esp_lcd_new_panel_ili9341
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle));

    // 重置、初始化、开启显示
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    
    // 很多 TFT 屏幕需要开启颜色反转才能正常显示，如果颜色发白发虚，请开启这行
    // ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, true)); 
    
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    ESP_LOGI(TAG, "点亮屏幕背光");
    // 配置背光 IO 为输出模式并拉高 (高电平点亮还是低电平点亮取决于屏幕底板电路，一般为高)
    gpio_config_t bk_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << TFT_BLK_PIN
    };
    ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));
    gpio_set_level(TFT_BLK_PIN, 1); // 1 开启背光，0 关闭

    ESP_LOGI(TAG, "TFT 屏幕初始化完成!");
    
    return panel_handle;
}