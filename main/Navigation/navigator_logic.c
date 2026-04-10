#include "navigator_logic.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include <math.h>

static const char *TAG = "NAVIGATOR";

// 定义圆周率
#define PI 3.14159265358979323846

// 假设 LED1 是正前方，顺时针排列
// 如果您的实际物理摆放不同，只需调整这个数组的顺序即可
static const int led_pins[8] = {
    PIN_LED_1, // 0°   (前)
    PIN_LED_2, // 45°  (右前)
    PIN_LED_3, // 90°  (右)
    PIN_LED_4, // 135° (右后)
    PIN_LED_5, // 180° (后)
    PIN_LED_6, // 225° (左后)
    PIN_LED_7, // 270° (左)
    PIN_LED_8  // 315° (左前)
};

static nav_mode_t current_mode = MODE_COMPASS;

// 初始化 LED PWM (LEDC)
void indicator_leds_init(void) {
    // 配置 LEDC 定时器
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_LOW_SPEED_MODE,
        .timer_num        = LEDC_TIMER_0,
        .duty_resolution  = LEDC_TIMER_13_BIT, // 0-8191
        .freq_hz          = 5000,              // 5 kHz
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    // 配置 8 个通道
    for (int i = 0; i < 8; i++) {
        ledc_channel_config_t ledc_channel = {
            .speed_mode     = LEDC_LOW_SPEED_MODE,
            .channel        = (ledc_channel_t)i,
            .timer_sel      = LEDC_TIMER_0,
            .intr_type      = LEDC_INTR_DISABLE,
            .gpio_num       = led_pins[i],
            .duty           = 0, // 初始亮度 0
            .hpoint         = 0
        };
        ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
    }
    
    // 初始化渐变功能（后续如果需要硬件级别的呼吸灯可以开启）
    ledc_fade_func_install(0);
    ESP_LOGI(TAG, "8个PWM指示灯初始化完成");
}

void set_navigation_mode(nav_mode_t mode) {
    current_mode = mode;
    if(mode == MODE_COMPASS) {
        ESP_LOGI(TAG, "已切换至: 🧭 指南针模式");
    } else {
        ESP_LOGI(TAG, "已切换至: 🚩 目标导航模式");
    }
}

// 角度转弧度
static double to_radians(double degrees) {
    return degrees * PI / 180.0;
}

// 计算两点间的绝对方位角 (从正北顺时针计算，0-360)
static double calculate_bearing(double lat1, double lon1, double lat2, double lon2) {
    double lat1_rad = to_radians(lat1);
    double lat2_rad = to_radians(lat2);
    double d_lon_rad = to_radians(lon2 - lon1);

    double y = sin(d_lon_rad) * cos(lat2_rad);
    double x = cos(lat1_rad) * sin(lat2_rad) - sin(lat1_rad) * cos(lat2_rad) * cos(d_lon_rad);
    
    double bearing = atan2(y, x) * 180.0 / PI;
    if (bearing < 0) {
        bearing += 360.0;
    }
    return bearing;
}

// 根据高斯分布(或其他缓和算法)点亮对应角度的LED
static void point_led_to_angle(float relative_angle) {
    // 将角度标准化到 0-359.99 之间
    while (relative_angle < 0) relative_angle += 360.0;
    while (relative_angle >= 360.0) relative_angle -= 360.0;

    // 动态亮度算法：根据每个灯与目标角度的夹角差，计算 PWM 亮度
    // 假设 LED1 在 0 度，LED2 在 45 度 ...
    for (int i = 0; i < 8; i++) {
        float led_angle = i * 45.0;
        float diff = fabs(relative_angle - led_angle);
        if (diff > 180.0) diff = 360.0 - diff; // 寻找最短角度差

        // 亮度随角度差衰减：在正对时最亮(8191)，超过 90 度就熄灭
        uint32_t duty = 0;
        if (diff < 90.0) {
            // 使用 cos 曲线让过渡更自然：cos(diff) 当 diff=0 时最大，diff=90 时为 0
            duty = (uint32_t)(8191 * cos(diff * PI / 180.0));
        }

        ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)i, duty);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)i);
    }
}

// 核心更新任务
void update_navigation_task(float current_heading, double cur_lat, double cur_lon, double target_lat, double target_lon) {
    double target_bearing = 0.0; // 默认指向正北

    if (current_mode == MODE_NAVIGATION) {
        // 计算目标点的绝对方向
        target_bearing = calculate_bearing(cur_lat, cur_lon, target_lat, target_lon);
        ESP_LOGI(TAG, "当前坐标:(%.6f, %.6f) 目标:(%.6f, %.6f) -> 目标方位角: %.1f°", 
                 cur_lat, cur_lon, target_lat, target_lon, target_bearing);
    }

    // 计算相对于设备前方的夹角
    // 相对角度 = 目标绝对角度 - 设备当前朝向角度
    float relative_angle = target_bearing - current_heading;

    if(current_mode == MODE_COMPASS) {
        ESP_LOGI(TAG, "当前设备朝向: %.1f° -> 相对正北在 %.1f° 方向", current_heading, relative_angle < 0 ? relative_angle + 360 : relative_angle);
    }

    point_led_to_angle(relative_angle);
}