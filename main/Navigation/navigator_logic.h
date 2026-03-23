#pragma once
#include <stdbool.h>

// 8个LED的引脚定义 (基于原理图)
#define PIN_LED_1 3
#define PIN_LED_2 46
#define PIN_LED_3 9
#define PIN_LED_4 10
#define PIN_LED_5 11
#define PIN_LED_6 12
#define PIN_LED_7 13
#define PIN_LED_8 45

// 导航状态枚举
typedef enum {
    MODE_COMPASS = 0, // 默认：指南针模式
    MODE_NAVIGATION   // 导航：指向目标点
} nav_mode_t;

// 初始化LED
void indicator_leds_init(void);

// 设置当前导航模式
void set_navigation_mode(nav_mode_t mode);

// 更新导航计算并刷新LED (需要在主循环或定时器中不断调用)
void update_navigation_task(float current_heading, double cur_lat, double cur_lon, double target_lat, double target_lon);

// 核心导航算法逻辑
// 我们要实现两种状态：
// 指南针模式（默认）：目标角度永远是“正北 (0°)”。根据地磁传感器获取当前设备的朝向角（Heading），我们要计算出“北”在设备的哪个相对方向。
// 公式： 相对角度 = (360 - 当前朝向角) % 360
// 导航模式：根据GPS的（当前经纬度）和（目标经纬度），使用大圆航线算法 (Haversine) 算出目标点的“绝对方位角 (Bearing)”。然后结合设备当前的朝向，算出应该指向哪个LED。
// 公式： 相对角度 = (绝对方位角 - 当前朝向角 + 360) % 360