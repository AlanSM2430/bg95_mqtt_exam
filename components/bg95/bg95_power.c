#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_err.h"

#include "bg95.h"
#include "bg95_internal.h"

static const char *TAG = "BG95_POWER";

esp_err_t bg95_powerkey_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << g_bg95_cfg.pwrkey_pin),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    ESP_ERROR_CHECK(gpio_config(&io_conf));

    // Reposo: PWRKEY liberado.
    gpio_set_level(g_bg95_cfg.pwrkey_pin, 1);

    ESP_LOGI(TAG, "PWRKEY configurado en GPIO%d", g_bg95_cfg.pwrkey_pin);

    return ESP_OK;
}

void bg95_power_on(void)
{
    ESP_LOGI(TAG, "Encendiendo BG95 con PWRKEY GPIO%d", g_bg95_cfg.pwrkey_pin);
    ESP_LOGI(TAG, "Secuencia: HIGH -> LOW 800ms -> HIGH");

    gpio_set_level(g_bg95_cfg.pwrkey_pin, 1);
    vTaskDelay(pdMS_TO_TICKS(1000));

    gpio_set_level(g_bg95_cfg.pwrkey_pin, 0);
    vTaskDelay(pdMS_TO_TICKS(800));

    gpio_set_level(g_bg95_cfg.pwrkey_pin, 1);

    ESP_LOGI(TAG, "Esperando arranque del modem...");
    vTaskDelay(pdMS_TO_TICKS(8000));
}

bool bg95_wait_at(int attempts)
{
    for (int i = 1; i <= attempts; i++) {
        ESP_LOGI(TAG, "Intento AT %d/%d", i, attempts);

        if (bg95_send_cmd("AT", 1000)) {
            ESP_LOGI(TAG, "BG95 respondió AT correctamente");
            return true;
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    return false;
}
