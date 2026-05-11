#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"

#include "bg95.h"
#include "bg95_internal.h"

static const char *TAG = "BG95_NET";

bool bg95_basic_info(void)
{
    bool ok = true;

    ok &= bg95_send_cmd("ATE0", 3000);
    ok &= bg95_send_cmd("ATI", 3000);
    ok &= bg95_send_cmd("AT+CGMI", 3000);
    ok &= bg95_send_cmd("AT+CGMM", 3000);
    ok &= bg95_send_cmd("AT+CPIN?", 3000);
    ok &= bg95_send_cmd("AT+CSQ", 3000);
    ok &= bg95_send_cmd("AT+CEREG?", 3000);
    ok &= bg95_send_cmd("AT+CGATT?", 3000);

    return ok;
}

bool bg95_check_sim(void)
{
    if (!bg95_send_cmd("AT+CPIN?", 3000)) {
        return false;
    }

    if (strstr(g_bg95_response, "+CPIN: READY")) {
        ESP_LOGI(TAG, "SIM READY");
        return true;
    }

    ESP_LOGE(TAG, "SIM no está READY");
    return false;
}

bool bg95_get_rssi(int *rssi)
{
    if (rssi == NULL) {
        return false;
    }

    *rssi = 99;

    if (!bg95_send_cmd("AT+CSQ", 3000)) {
        return false;
    }

    char *p = strstr(g_bg95_response, "+CSQ:");
    if (!p) {
        return false;
    }

    int ber = 99;

    if (sscanf(p, "+CSQ: %d,%d", rssi, &ber) == 2) {
        ESP_LOGI(TAG, "RSSI CSQ = %d", *rssi);
        return true;
    }

    return false;
}

static bool bg95_is_registered_from_cereg(void)
{
    char *p = strstr(g_bg95_response, "+CEREG:");
    if (!p) {
        return false;
    }

    int n = -1;
    int stat = -1;

    if (sscanf(p, "+CEREG: %d,%d", &n, &stat) == 2) {
        ESP_LOGI(TAG, "CEREG stat = %d", stat);

        if (stat == 1 || stat == 5) {
            return true;
        }
    }

    return false;
}

bool bg95_wait_registration(int timeout_seconds)
{
    ESP_LOGI(TAG, "Esperando registro LTE...");

    bg95_send_cmd("AT+CEREG=2", 3000);

    int elapsed = 0;

    while (elapsed < timeout_seconds) {
        ESP_LOGI(TAG, "Consultando CEREG... %d/%d s", elapsed, timeout_seconds);

        if (bg95_send_cmd("AT+CEREG?", 3000)) {
            if (bg95_is_registered_from_cereg()) {
                ESP_LOGI(TAG, "REGISTRADO EN RED CELULAR");
                return true;
            }
        }

        bg95_send_cmd("AT+CSQ", 3000);
        bg95_send_cmd("AT+CGATT?", 3000);

        vTaskDelay(pdMS_TO_TICKS(5000));
        elapsed += 5;
    }

    ESP_LOGE(TAG, "No se logró registro LTE");
    return false;
}

bool bg95_activate_pdp(void)
{
    char cmd[256];

    ESP_LOGI(TAG, "Configurando APN y activando PDP");

    bg95_send_cmd("AT+CGATT?", 3000);

    snprintf(cmd, sizeof(cmd),
             "AT+CGDCONT=1,\"IP\",\"%s\"",
             g_bg95_cfg.apn);

    bg95_send_cmd(cmd, 5000);

    snprintf(cmd, sizeof(cmd),
             "AT+QICSGP=1,1,\"%s\",\"%s\",\"%s\",1",
             g_bg95_cfg.apn,
             g_bg95_cfg.apn_user,
             g_bg95_cfg.apn_pass);

    if (!bg95_send_cmd(cmd, 5000)) {
        ESP_LOGE(TAG, "Falló QICSGP");
        return false;
    }

    bg95_send_cmd("AT+QIDEACT=1", 20000);
    vTaskDelay(pdMS_TO_TICKS(1000));

    if (!bg95_send_cmd("AT+QIACT=1", 90000)) {
        ESP_LOGW(TAG, "QIACT devolvió error o timeout");
        bg95_send_cmd("AT+QIGETERROR", 5000);
        return false;
    }

    return true;
}

bool bg95_get_ip(char *ip_out, size_t ip_out_size)
{
    if (ip_out == NULL || ip_out_size == 0) {
        return false;
    }

    memset(ip_out, 0, ip_out_size);

    if (!bg95_send_cmd("AT+QIACT?", 10000)) {
        return false;
    }

    char *p = strstr(g_bg95_response, "+QIACT:");
    if (!p) {
        return false;
    }

    char *q1 = strchr(p, '"');
    if (!q1) {
        return false;
    }

    char *q2 = strchr(q1 + 1, '"');
    if (!q2) {
        return false;
    }

    int len = q2 - q1 - 1;

    if (len <= 0 || len >= (int)ip_out_size) {
        return false;
    }

    memcpy(ip_out, q1 + 1, len);
    ip_out[len] = '\0';

    ESP_LOGI(TAG, "IP OBTENIDA: %s", ip_out);

    return true;
}

bool bg95_network_start(char *ip_out, size_t ip_out_size)
{
    if (!bg95_check_sim()) {
        return false;
    }

    if (!bg95_wait_registration(120)) {
        return false;
    }

    if (!bg95_activate_pdp()) {
        return false;
    }

    if (!bg95_get_ip(ip_out, ip_out_size)) {
        return false;
    }

    return true;
}
