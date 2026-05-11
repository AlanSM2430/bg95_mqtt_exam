#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/uart.h"
#include "esp_log.h"

#include "bg95.h"
#include "bg95_internal.h"

static const char *TAG = "BG95_MQTT";

static int s_msg_id = 1;

static bool mqtt_parse_open_ok(void)
{
    char *p = strstr(g_bg95_response, "+QMTOPEN:");
    if (!p) {
        return false;
    }

    int client = -1;
    int result = -1;

    if (sscanf(p, "+QMTOPEN: %d,%d", &client, &result) == 2) {
        ESP_LOGI(TAG, "QMTOPEN client=%d result=%d", client, result);
        return result == 0;
    }

    return false;
}

static bool mqtt_parse_conn_ok(void)
{
    char *p = strstr(g_bg95_response, "+QMTCONN:");
    if (!p) {
        return false;
    }

    int client = -1;
    int result = -1;
    int ret_code = -1;

    if (sscanf(p, "+QMTCONN: %d,%d,%d", &client, &result, &ret_code) >= 2) {
        ESP_LOGI(TAG, "QMTCONN client=%d result=%d ret=%d", client, result, ret_code);
        return result == 0;
    }

    return false;
}

static bool mqtt_parse_sub_ok(void)
{
    char *p = strstr(g_bg95_response, "+QMTSUB:");
    if (!p) {
        return false;
    }

    int client = -1;
    int msgid = -1;
    int result = -1;
    int qos = -1;

    if (sscanf(p, "+QMTSUB: %d,%d,%d,%d", &client, &msgid, &result, &qos) >= 3) {
        ESP_LOGI(TAG, "QMTSUB client=%d msgid=%d result=%d qos=%d", client, msgid, result, qos);
        return result == 0;
    }

    return false;
}

bool bg95_mqtt_open(const char *host, int port)
{
    char cmd[256];

    if (host == NULL) {
        return false;
    }

    ESP_LOGI(TAG, "Abriendo MQTT hacia %s:%d", host, port);

    bg95_send_cmd("AT+QMTCLOSE=0", 10000);
    vTaskDelay(pdMS_TO_TICKS(1000));

    // Recibir mensajes por URC +QMTRECV.
    bg95_send_cmd("AT+QMTCFG=\"recv/mode\",0,0,1", 3000);
    bg95_send_cmd("AT+QMTCFG=\"keepalive\",0,60", 3000);

    snprintf(cmd, sizeof(cmd),
             "AT+QMTOPEN=0,\"%s\",%d",
             host,
             port);

    if (!bg95_send_cmd_expect(cmd, "+QMTOPEN:", 60000)) {
        ESP_LOGE(TAG, "No llegó +QMTOPEN");
        return false;
    }

    return mqtt_parse_open_ok();
}

bool bg95_mqtt_connect(const char *client_id)
{
    char cmd[256];

    if (client_id == NULL) {
        return false;
    }

    snprintf(cmd, sizeof(cmd),
             "AT+QMTCONN=0,\"%s\"",
             client_id);

    if (!bg95_send_cmd_expect(cmd, "+QMTCONN:", 30000)) {
        ESP_LOGE(TAG, "No llegó +QMTCONN");
        return false;
    }

    return mqtt_parse_conn_ok();
}

bool bg95_mqtt_subscribe(const char *topic)
{
    char cmd[256];

    if (topic == NULL) {
        return false;
    }

    snprintf(cmd, sizeof(cmd),
             "AT+QMTSUB=0,%d,\"%s\",1",
             s_msg_id++,
             topic);

    if (!bg95_send_cmd_expect(cmd, "+QMTSUB:", 30000)) {
        ESP_LOGE(TAG, "No llegó +QMTSUB");
        return false;
    }

    return mqtt_parse_sub_ok();
}

static bool wait_for_prompt_or_error(int timeout_ms)
{
    memset(g_bg95_response, 0, BG95_RESPONSE_SIZE);

    int total = 0;
    TickType_t start = xTaskGetTickCount();
    TickType_t timeout_ticks = pdMS_TO_TICKS(timeout_ms);

    while ((xTaskGetTickCount() - start) < timeout_ticks &&
           total < BG95_RESPONSE_SIZE - 1) {

        int len = uart_read_bytes(
            g_bg95_cfg.uart_port,
            (uint8_t *)(g_bg95_response + total),
            BG95_RESPONSE_SIZE - total - 1,
            pdMS_TO_TICKS(200)
        );

        if (len > 0) {
            total += len;
            g_bg95_response[total] = '\0';

            if (strchr(g_bg95_response, '>')) {
                ESP_LOGI(TAG, "Prompt MQTT recibido");
                return true;
            }

            if (strstr(g_bg95_response, "ERROR") ||
                strstr(g_bg95_response, "+CME ERROR")) {
                ESP_LOGE(TAG, "Error esperando prompt:\n%s", g_bg95_response);
                return false;
            }
        }
    }

    ESP_LOGE(TAG, "Timeout esperando prompt MQTT");
    return false;
}

bool bg95_mqtt_publish(const char *topic, const char *payload, int qos, int retain)
{
    char cmd[256];

    if (topic == NULL || payload == NULL) {
        return false;
    }

    // Publicación en modo prompt:
    // AT+QMTPUB=0,msgid,qos,retain,"topic"
    // Esperar '>'
    // Enviar payload
    // Finalizar con Ctrl+Z (0x1A)
    snprintf(cmd, sizeof(cmd),
             "AT+QMTPUB=0,%d,%d,%d,\"%s\"",
             s_msg_id++,
             qos,
             retain,
             topic);

    uart_flush_input(g_bg95_cfg.uart_port);

    ESP_LOGI(TAG, "TX: %s", cmd);

    uart_write_bytes(g_bg95_cfg.uart_port, cmd, strlen(cmd));
    uart_write_bytes(g_bg95_cfg.uart_port, "\r\n", 2);

    if (!wait_for_prompt_or_error(5000)) {
        ESP_LOGE(TAG, "No llegó prompt > para publicar MQTT");
        return false;
    }

    ESP_LOGI(TAG, "MQTT payload: %s", payload);

    uart_write_bytes(g_bg95_cfg.uart_port, payload, strlen(payload));

    uint8_t ctrl_z = 0x1A;
    uart_write_bytes(g_bg95_cfg.uart_port, (const char *)&ctrl_z, 1);

    memset(g_bg95_response, 0, BG95_RESPONSE_SIZE);

    int total = 0;
    TickType_t start = xTaskGetTickCount();
    TickType_t timeout_ticks = pdMS_TO_TICKS(30000);

    while ((xTaskGetTickCount() - start) < timeout_ticks &&
           total < BG95_RESPONSE_SIZE - 1) {

        int len = uart_read_bytes(
            g_bg95_cfg.uart_port,
            (uint8_t *)(g_bg95_response + total),
            BG95_RESPONSE_SIZE - total - 1,
            pdMS_TO_TICKS(200)
        );

        if (len > 0) {
            total += len;
            g_bg95_response[total] = '\0';

            if (strstr(g_bg95_response, "+QMTPUB:")) {
                ESP_LOGI(TAG, "RX:\n%s", g_bg95_response);

                int client = -1;
                int msgid = -1;
                int result = -1;

                char *p = strstr(g_bg95_response, "+QMTPUB:");
                if (p && sscanf(p, "+QMTPUB: %d,%d,%d", &client, &msgid, &result) == 3) {
                    ESP_LOGI(TAG, "QMTPUB client=%d msgid=%d result=%d", client, msgid, result);
                    return result == 0;
                }

                return true;
            }

            if (strstr(g_bg95_response, "ERROR") ||
                strstr(g_bg95_response, "+CME ERROR")) {
                ESP_LOGE(TAG, "Error publicando MQTT:\n%s", g_bg95_response);
                return false;
            }
        }
    }

    ESP_LOGE(TAG, "Timeout esperando +QMTPUB");
    return false;
}

bool bg95_mqtt_close(void)
{
    bg95_send_cmd("AT+QMTDISC=0", 10000);
    bg95_send_cmd("AT+QMTCLOSE=0", 10000);
    return true;
}

bool bg95_mqtt_poll_command(char *out, size_t out_size, int timeout_ms)
{
    if (out == NULL || out_size == 0) {
        return false;
    }

    memset(out, 0, out_size);

    char raw[1024];

    int len = bg95_read_raw(raw, sizeof(raw), timeout_ms);

    if (len <= 0) {
        return false;
    }

    if (strstr(raw, "+QMTRECV") ||
        strstr(raw, "+QMTSTAT") ||
        strstr(raw, "+QMTDISC")) {

        strncpy(out, raw, out_size - 1);
        out[out_size - 1] = '\0';

        return true;
    }

    return false;
}
