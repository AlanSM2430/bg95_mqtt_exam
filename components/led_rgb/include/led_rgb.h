#pragma once

#include "esp_err.h"
#include "driver/gpio.h"

typedef enum {
    LED_RGB_OFF = 0,
    LED_RGB_RED,
    LED_RGB_GREEN,
    LED_RGB_BLUE,
    LED_RGB_YELLOW,
    LED_RGB_CYAN,
    LED_RGB_MAGENTA,
    LED_RGB_WHITE
} led_rgb_color_t;

typedef struct {
    gpio_num_t red_pin;
    gpio_num_t green_pin;
    gpio_num_t blue_pin;
    int active_level;
} led_rgb_config_t;

esp_err_t led_rgb_init(const led_rgb_config_t *config);
void led_rgb_set(led_rgb_color_t color);
led_rgb_color_t led_rgb_get_current(void);
const char *led_rgb_color_name(led_rgb_color_t color);
