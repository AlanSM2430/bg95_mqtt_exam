#include <string.h>

#include "driver/gpio.h"
#include "esp_log.h"

#include "led_rgb.h"

static const char *TAG = "LED_RGB";

static led_rgb_config_t s_cfg;
static led_rgb_color_t s_current_color = LED_RGB_OFF;

static void write_leds(int red_on, int green_on, int blue_on)
{
    int off = s_cfg.active_level ? 0 : 1;
    int on = s_cfg.active_level ? 1 : 0;

    gpio_set_level(s_cfg.red_pin, red_on ? on : off);
    gpio_set_level(s_cfg.green_pin, green_on ? on : off);
    gpio_set_level(s_cfg.blue_pin, blue_on ? on : off);
}

esp_err_t led_rgb_init(const led_rgb_config_t *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(&s_cfg, config, sizeof(led_rgb_config_t));

    gpio_config_t io_conf = {
        .pin_bit_mask =
            (1ULL << s_cfg.red_pin) |
            (1ULL << s_cfg.green_pin) |
            (1ULL << s_cfg.blue_pin),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    ESP_ERROR_CHECK(gpio_config(&io_conf));

    led_rgb_set(LED_RGB_OFF);

    ESP_LOGI(TAG, "LED RGB inicializado R=%d G=%d B=%d",
             s_cfg.red_pin,
             s_cfg.green_pin,
             s_cfg.blue_pin);

    return ESP_OK;
}

void led_rgb_set(led_rgb_color_t color)
{
    switch (color) {
        case LED_RGB_RED:
            write_leds(1, 0, 0);
            break;

        case LED_RGB_GREEN:
            write_leds(0, 1, 0);
            break;

        case LED_RGB_BLUE:
            write_leds(0, 0, 1);
            break;

        case LED_RGB_YELLOW:
            write_leds(1, 1, 0);
            break;

        case LED_RGB_CYAN:
            write_leds(0, 1, 1);
            break;

        case LED_RGB_MAGENTA:
            write_leds(1, 0, 1);
            break;

        case LED_RGB_WHITE:
            write_leds(1, 1, 1);
            break;

        case LED_RGB_OFF:
        default:
            write_leds(0, 0, 0);
            color = LED_RGB_OFF;
            break;
    }

    s_current_color = color;
}

led_rgb_color_t led_rgb_get_current(void)
{
    return s_current_color;
}

const char *led_rgb_color_name(led_rgb_color_t color)
{
    switch (color) {
        case LED_RGB_RED:
            return "red";
        case LED_RGB_GREEN:
            return "green";
        case LED_RGB_BLUE:
            return "blue";
        case LED_RGB_YELLOW:
            return "yellow";
        case LED_RGB_CYAN:
            return "cyan";
        case LED_RGB_MAGENTA:
            return "magenta";
        case LED_RGB_WHITE:
            return "white";
        case LED_RGB_OFF:
        default:
            return "off";
    }
}
