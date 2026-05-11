#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/uart.h"
#include "esp_log.h"
#include "esp_err.h"

#include "bg95.h"
#include "bg95_internal.h"

static const char *TAG = "BG95_UART";

bg95_config_t g_bg95_cfg;
char g_bg95_response[BG95_RESPONSE_SIZE];

esp_err_t bg95_init(const bg95_config_t *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(&g_bg95_cfg, 0, sizeof(g_bg95_cfg));
    memcpy(&g_bg95_cfg, config, sizeof(bg95_config_t));

    uart_config_t uart_config = {
        .baud_rate = g_bg95_cfg.baud_rate,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_driver_install(
        g_bg95_cfg.uart_port,
        4096,
        4096,
        0,
        NULL,
        0
    ));

    ESP_ERROR_CHECK(uart_param_config(g_bg95_cfg.uart_port, &uart_config));

    ESP_ERROR_CHECK(uart_set_pin(
        g_bg95_cfg.uart_port,
        g_bg95_cfg.tx_pin,
        g_bg95_cfg.rx_pin,
        UART_PIN_NO_CHANGE,
        UART_PIN_NO_CHANGE
    ));

    uart_flush_input(g_bg95_cfg.uart_port);

    ESP_LOGI(TAG, "UART inicializada");
    ESP_LOGI(TAG, "TX GPIO%d", g_bg95_cfg.tx_pin);
    ESP_LOGI(TAG, "RX GPIO%d", g_bg95_cfg.rx_pin);
    ESP_LOGI(TAG, "Baudrate %d", g_bg95_cfg.baud_rate);

    return ESP_OK;
}

bool bg95_send_cmd(const char *cmd, int timeout_ms)
{
    return bg95_send_cmd_expect(cmd, NULL, timeout_ms);
}

bool bg95_send_cmd_expect(const char *cmd, const char *expect, int timeout_ms)
{
    if (cmd == NULL) {
        return false;
    }

    memset(g_bg95_response, 0, sizeof(g_bg95_response));
    uart_flush_input(g_bg95_cfg.uart_port);

    ESP_LOGI(TAG, "TX: %s", cmd);

    uart_write_bytes(g_bg95_cfg.uart_port, cmd, strlen(cmd));
    uart_write_bytes(g_bg95_cfg.uart_port, "\r\n", 2);

    int total = 0;
    TickType_t start = xTaskGetTickCount();
    TickType_t timeout_ticks = pdMS_TO_TICKS(timeout_ms);

    while ((xTaskGetTickCount() - start) < timeout_ticks &&
           total < (int)sizeof(g_bg95_response) - 1) {

        int len = uart_read_bytes(
            g_bg95_cfg.uart_port,
            (uint8_t *)(g_bg95_response + total),
            sizeof(g_bg95_response) - total - 1,
            pdMS_TO_TICKS(200)
        );

        if (len > 0) {
            total += len;
            g_bg95_response[total] = '\0';

            if (expect != NULL) {
                if (strstr(g_bg95_response, expect) != NULL) {
                    ESP_LOGI(TAG, "RX:\n%s", g_bg95_response);
                    return true;
                }
            } else {
                if (strstr(g_bg95_response, "\r\nOK\r\n") ||
                    strstr(g_bg95_response, "\nOK\r\n")) {
                    ESP_LOGI(TAG, "RX:\n%s", g_bg95_response);
                    return true;
                }
            }

            if (strstr(g_bg95_response, "\r\nERROR\r\n") ||
                strstr(g_bg95_response, "\nERROR\r\n") ||
                strstr(g_bg95_response, "+CME ERROR") ||
                strstr(g_bg95_response, "+CMS ERROR")) {
                ESP_LOGW(TAG, "RX ERROR:\n%s", g_bg95_response);
                return false;
            }
        }
    }

    if (total > 0) {
        ESP_LOGW(TAG, "RX timeout/parcial:\n%s", g_bg95_response);
    } else {
        ESP_LOGW(TAG, "RX: sin respuesta");
    }

    return false;
}

int bg95_read_raw(char *out, size_t out_size, int timeout_ms)
{
    if (out == NULL || out_size == 0) {
        return 0;
    }

    memset(out, 0, out_size);

    int len = uart_read_bytes(
        g_bg95_cfg.uart_port,
        (uint8_t *)out,
        out_size - 1,
        pdMS_TO_TICKS(timeout_ms)
    );

    if (len > 0) {
        out[len] = '\0';
        ESP_LOGI(TAG, "URC RX:\n%s", out);
    }

    return len;
}

const char *bg95_last_response(void)
{
    return g_bg95_response;
}
