#include <stdint.h>
#include "audio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"
#include "freertos/task.h"

#include "driver/i2s_std.h"

// ---------- 引脚定义 (基于您的原理图) ----------
#define I2S_BCLK_PIN  15  // 共用时钟
#define I2S_LRC_PIN   16  // 共用左右声道选择(WS)
#define I2S_DOUT_PIN  6   // 输出到功放 (MAX98357 DIN)
#define I2S_DIN_PIN   7   // 输入自麦克风 (INMP441 SD)

// 采样率和位深
#define SAMPLE_RATE   16000 // 根据您的业务需求修改(如44100, 16000)
#define BITS_PER_SAMPLE I2S_DATA_BIT_WIDTH_32BIT // INMP441建议使用32位读取以保证时序

// I2S 通道句柄
i2s_chan_handle_t tx_chan; // 发送(播放)通道
i2s_chan_handle_t rx_chan; // 接收(录音)通道

void audio_i2s_init() {
    // 1. 分配 I2S 通道 (全双工，自动选择 I2S 端口，ESP32 作为 Master)
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_chan, &rx_chan));

    // 2. 配置 I2S 标准模式参数
    i2s_std_config_t std_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        // 配置为立体声模式 (I2S_SLOT_MODE_STEREO)，32位宽
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(BITS_PER_SAMPLE, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED, // 您的模块不需要 MCLK
            .bclk = I2S_BCLK_PIN,
            .ws   = I2S_LRC_PIN,
            .dout = I2S_DOUT_PIN,
            .din  = I2S_DIN_PIN,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv   = false,
            },
        },
    };

    // 3. 初始化 TX 和 RX 通道
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_chan, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_chan, &std_cfg));

    // 4. 启用通道
    ESP_ERROR_CHECK(i2s_channel_enable(tx_chan));
    ESP_ERROR_CHECK(i2s_channel_enable(rx_chan));
    
    // 初始化完成！
}