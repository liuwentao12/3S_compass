#include "navigator_logic.h"
#include "driver/gpio.h"
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

// 初始化 GPIO
void indicator_leds_init(void) {
    for (int i = 0; i < 8; i++) {
        gpio_reset_pin(led_pins[i]);
        gpio_set_direction(led_pins[i], GPIO_MODE_OUTPUT);
        gpio_set_level(led_pins[i], 0); // 默认全灭
    }
    ESP_LOGI(TAG, "8个指示灯初始化完成");
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

// 点亮对应角度的LED
static void point_led_to_angle(float relative_angle) {
    // 将角度标准化到 0-359.99 之间
    while (relative_angle < 0) relative_angle += 360.0;
    while (relative_angle >= 360.0) relative_angle -= 360.0;

    // 将 360 度分为 8 个区间，每个区间 45 度 (偏移 22.5 度以保证四舍五入到最近的灯)
    int led_index = (int)((relative_angle + 22.5) / 45.0) % 8;

    // 刷新 LED 状态：只点亮目标灯，熄灭其他灯
    for (int i = 0; i < 8; i++) {
        gpio_set_level(led_pins[i], (i == led_index) ? 1 : 0);
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