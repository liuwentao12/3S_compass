#include "interaction/audio.h"
#include "interaction/tft_display.h"
#include "mini_claw/mini_config.h"
#include "Navigation/sensor_modules.h"
#include "Navigation/navigator_logic.h"

void app_main(void)
{
    audio_i2s_init();
    tft_display_init();
    compass_i2c_init();
    gps_uart_init();

    // 1. 初始化指示灯
    indicator_leds_init();

    // 默认是指南针模式
    set_navigation_mode(MODE_COMPASS);

    // 假设这些是你要去的目标地 (比如北京故宫)
    double target_lat = 39.916345;
    double target_lon = 116.397155;

    // 模拟的当前位置 (假设在上海)
    double current_lat = 31.230416;
    double current_lon = 121.473701;
    // 模拟设备的实时朝向 (0是正北，90是正东，180是正南)
    float fake_heading = 0.0; 
    while (1)
    {
        // 【模拟功能1】：设备正在原地慢慢旋转 (每秒转 45 度)
        fake_heading += 45.0;
        if(fake_heading >= 360.0) fake_heading -= 360.0;

        // 【模拟功能2】：测试从串口按下按钮（这里用定时器模拟，10秒后自动切换到导航模式）
        static int tick = 0;
        tick++;
        if (tick == 10) {
            set_navigation_mode(MODE_NAVIGATION); // 切换为导航指向目标
        }

        // 核心更新：传入当前朝向、当前坐标、目标坐标
        update_navigation_task(fake_heading, current_lat, current_lon, target_lat, target_lon);

        vTaskDelay(pdMS_TO_TICKS(1000)); // 每秒刷新一次
    }
    
}