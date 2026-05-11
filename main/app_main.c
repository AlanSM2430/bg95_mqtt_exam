#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"

#include "esp_log.h"

#include "bg95.h"
#include "led_rgb.h"
#include "telemetry.h"

/* =========================================================
   CONFIG HARDWARE VALIDADA
   ========================================================= */

#define MODEM_UART_PORT      UART_NUM_1
#define MODEM_TX_PIN         GPIO_NUM_8
#define MODEM_RX_PIN         GPIO_NUM_7
#define MODEM_PWRKEY_PIN     GPIO_NUM_15
#define MODEM_BAUD_RATE      115200

#define LED_RED_PIN          GPIO_NUM_4
#define LED_GREEN_PIN        GPIO_NUM_5
#define LED_BLUE_PIN         GPIO_NUM_11

// Cambia a 0 si tu LED RGB es activo en bajo.
#define LED_ACTIVE_LEVEL     0

/* =========================================================
   CONFIG CELULAR
   ========================================================= */

#define APN                  "internet.itelcel.com"
#define APN_USER             "webgprs"
#define APN_PASS             "webgprs2002"

/* =========================================================
   CONFIG MQTT
   ========================================================= */

#define DEVICE_ID            "Alan Sandoval_esp32c6_bg95"

#define MQTT_HOST            "broker.hivemq.com"
#define MQTT_PORT            1883

#define MQTT_TOPIC_DATA      "dispositivo/Alan Sandoval/data"
#define MQTT_TOPIC_CMD       "dispositivo/Alan Sandoval/cmd"
#define MQTT_TOPIC_STATUS    "dispositivo/Alan Sandoval/status"

#define TELEMETRY_PERIOD_MS  60000

static const char *TAG = "APP_MAIN";

/* =========================================================
   EVENTOS / COLAS
   ========================================================= */

#define APP_EVENT_MQTT_READY BIT0

typedef enum {
    MODEM_REQ_PUBLISH_TELEMETRY = 1,
    MODEM_REQ_PUBLISH_STATUS
} modem_request_type_t;

typedef struct {
    modem_request_type_t type;
} modem_request_t;

typedef struct {
    led_rgb_color_t color;
} led_request_t;

static EventGroupHandle_t g_app_events;
static QueueHandle_t g_modem_queue;
static QueueHandle_t g_led_queue;

static char g_ip[64];

/* =========================================================
   HELPERS
   ========================================================= */

static bool cmd_contains(const char *cmd, const char *token)
{
    if (cmd == NULL || token == NULL) {
        return false;
    }

    return strstr(cmd, token) != NULL;
}

static bool parse_led_command(const char *cmd, led_rgb_color_t *out_color)
{
    if (cmd == NULL || out_color == NULL) {
        return false;
    }

    if (cmd_contains(cmd, "red")) {
        *out_color = LED_RGB_RED;
        return true;
    }

    if (cmd_contains(cmd, "green")) {
        *out_color = LED_RGB_GREEN;
        return true;
    }

    if (cmd_contains(cmd, "blue")) {
        *out_color = LED_RGB_BLUE;
        return true;
    }

    if (cmd_contains(cmd, "yellow")) {
        *out_color = LED_RGB_YELLOW;
        return true;
    }

    if (cmd_contains(cmd, "cyan")) {
        *out_color = LED_RGB_CYAN;
        return true;
    }

    if (cmd_contains(cmd, "magenta")) {
        *out_color = LED_RGB_MAGENTA;
        return true;
    }

    if (cmd_contains(cmd, "white")) {
        *out_color = LED_RGB_WHITE;
        return true;
    }

    if (cmd_contains(cmd, "off")) {
        *out_color = LED_RGB_OFF;
        return true;
    }

    return false;
}

static bool is_status_command(const char *cmd)
{
    return cmd_contains(cmd, "status");
}

/* =========================================================
   PUBLICACIONES MQTT
   ========================================================= */

static bool publish_telemetry(void)
{
    char payload[512];
    char ip[64];
    int rssi = 99;

    memset(ip, 0, sizeof(ip));

    bg95_get_rssi(&rssi);

    if (bg95_get_ip(ip, sizeof(ip))) {
        strncpy(g_ip, ip, sizeof(g_ip) - 1);
        g_ip[sizeof(g_ip) - 1] = '\0';
    }

    telemetry_build_json(
        payload,
        sizeof(payload),
        DEVICE_ID,
        rssi,
        g_ip,
        led_rgb_color_name(led_rgb_get_current())
    );

    ESP_LOGI(TAG, "Publicando telemetría: %s", payload);

    return bg95_mqtt_publish(MQTT_TOPIC_DATA, payload, 1, 0);
}

static bool publish_status(const char *event)
{
    char payload[512];
    int rssi = 99;

    bg95_get_rssi(&rssi);

    snprintf(
        payload,
        sizeof(payload),
        "{"
            "\"device\":\"%s\","
            "\"event\":\"%s\","
            "\"mqtt\":\"connected\","
            "\"network\":\"registered\","
            "\"ip\":\"%s\","
            "\"rssi\":%d,"
            "\"led\":\"%s\""
        "}",
        DEVICE_ID,
        event ? event : "status",
        g_ip,
        rssi,
        led_rgb_color_name(led_rgb_get_current())
    );

    ESP_LOGI(TAG, "Publicando status: %s", payload);

    return bg95_mqtt_publish(MQTT_TOPIC_STATUS, payload, 1, 0);
}

/* =========================================================
   LED TASK
   ========================================================= */

static void led_task(void *arg)
{
    led_request_t req;

    while (1) {
        if (xQueueReceive(g_led_queue, &req, portMAX_DELAY) == pdTRUE) {
            led_rgb_set(req.color);

            ESP_LOGI(TAG, "LED actualizado a: %s",
                     led_rgb_color_name(req.color));
        }
    }
}

/* =========================================================
   TELEMETRY TASK
   Solo solicita publicación.
   No habla directamente con el módem.
   ========================================================= */

static void telemetry_task(void *arg)
{
    modem_request_t req = {
        .type = MODEM_REQ_PUBLISH_TELEMETRY
    };

    while (1) {
        xEventGroupWaitBits(
            g_app_events,
            APP_EVENT_MQTT_READY,
            pdFALSE,
            pdTRUE,
            portMAX_DELAY
        );

        vTaskDelay(pdMS_TO_TICKS(TELEMETRY_PERIOD_MS));

        if (xEventGroupGetBits(g_app_events) & APP_EVENT_MQTT_READY) {
            xQueueSend(g_modem_queue, &req, 0);
        }
    }
}

/* =========================================================
   PROCESAMIENTO DE MENSAJES MQTT ENTRANTES
   ========================================================= */

static void process_mqtt_urc(const char *urc)
{
    if (urc == NULL) {
        return;
    }

    ESP_LOGI(TAG, "Procesando MQTT URC:\n%s", urc);

    led_rgb_color_t color;

    if (parse_led_command(urc, &color)) {
        led_request_t led_req = {
            .color = color
        };

        xQueueSend(g_led_queue, &led_req, 0);

        publish_status("led_updated");
        return;
    }

    if (is_status_command(urc)) {
        publish_status("status_requested");
        return;
    }

    ESP_LOGW(TAG, "Comando MQTT no reconocido");
    publish_status("unknown_command");
}

/* =========================================================
   CONEXIÓN COMPLETA MODEM + RED + MQTT
   ========================================================= */

static bool modem_connect_network_and_mqtt(void)
{
    memset(g_ip, 0, sizeof(g_ip));

    ESP_LOGI(TAG, "Verificando módem...");

    if (!bg95_wait_at(5)) {
        ESP_LOGW(TAG, "El módem no responde. Intentando PWRKEY...");

        bg95_power_on();

        if (!bg95_wait_at(30)) {
            ESP_LOGE(TAG, "El módem no respondió después de PWRKEY");
            return false;
        }
    }

    bg95_basic_info();

    // NET_STATUS LED mode del BG95. No es obligatorio para datos, pero ayuda en diagnóstico.
    bg95_send_cmd("AT+QCFG=\"ledmode\",1", 3000);
    bg95_send_cmd("AT+QCFG=\"ledmode\"", 3000);

    ESP_LOGI(TAG, "Iniciando red celular...");

    if (!bg95_network_start(g_ip, sizeof(g_ip))) {
        ESP_LOGE(TAG, "Falló inicio de red celular");
        return false;
    }

    ESP_LOGI(TAG, "IP celular: %s", g_ip);

    ESP_LOGI(TAG, "Conectando MQTT...");

    if (!bg95_mqtt_open(MQTT_HOST, MQTT_PORT)) {
        ESP_LOGE(TAG, "Falló QMTOPEN");
        return false;
    }

    if (!bg95_mqtt_connect(DEVICE_ID)) {
        ESP_LOGE(TAG, "Falló QMTCONN");
        return false;
    }

    if (!bg95_mqtt_subscribe(MQTT_TOPIC_CMD)) {
        ESP_LOGE(TAG, "Falló QMTSUB");
        return false;
    }

    ESP_LOGI(TAG, "MQTT conectado y suscrito a: %s", MQTT_TOPIC_CMD);

    return true;
}

/* =========================================================
   MODEM TASK
   Única tarea que habla con el módem.
   ========================================================= */

static void modem_task(void *arg)
{
    char mqtt_urc[1024];
    modem_request_t req;

    while (1) {
        led_rgb_set(LED_RGB_BLUE);

        xEventGroupClearBits(g_app_events, APP_EVENT_MQTT_READY);

        if (!modem_connect_network_and_mqtt()) {
            ESP_LOGE(TAG, "Fallo conexión. Reintentando en 10s...");
            led_rgb_set(LED_RGB_RED);
            bg95_mqtt_close();
            vTaskDelay(pdMS_TO_TICKS(10000));
            continue;
        }

        led_rgb_set(LED_RGB_GREEN);

        xEventGroupSetBits(g_app_events, APP_EVENT_MQTT_READY);

        publish_status("online");
        publish_telemetry();

        bool mqtt_ok = true;

        while (mqtt_ok) {
            if (xQueueReceive(g_modem_queue, &req, pdMS_TO_TICKS(500)) == pdTRUE) {
                switch (req.type) {
                    case MODEM_REQ_PUBLISH_TELEMETRY:
                        if (!publish_telemetry()) {
                            ESP_LOGW(TAG, "Falló publicación de telemetría");
                            mqtt_ok = false;
                        }
                        break;

                    case MODEM_REQ_PUBLISH_STATUS:
                        if (!publish_status("manual_status")) {
                            ESP_LOGW(TAG, "Falló publicación de status");
                            mqtt_ok = false;
                        }
                        break;

                    default:
                        break;
                }
            }

            memset(mqtt_urc, 0, sizeof(mqtt_urc));

            if (bg95_mqtt_poll_command(mqtt_urc, sizeof(mqtt_urc), 100)) {
                if (cmd_contains(mqtt_urc, "+QMTSTAT") ||
                    cmd_contains(mqtt_urc, "+QMTDISC")) {
                    ESP_LOGW(TAG, "MQTT desconectado por URC");
                    mqtt_ok = false;
                    break;
                }

                process_mqtt_urc(mqtt_urc);
            }
        }

        xEventGroupClearBits(g_app_events, APP_EVENT_MQTT_READY);

        led_rgb_set(LED_RGB_RED);

        ESP_LOGW(TAG, "Reconectando MQTT/red...");

        bg95_mqtt_close();

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

/* =========================================================
   MAIN
   ========================================================= */

void app_main(void)
{
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "   ESP32-C6 + BG95-M3 MQTT EXAM");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "UART TX     = GPIO8");
    ESP_LOGI(TAG, "UART RX     = GPIO7");
    ESP_LOGI(TAG, "PWRKEY      = GPIO15");
    ESP_LOGI(TAG, "LED RED     = GPIO4");
    ESP_LOGI(TAG, "LED GREEN   = GPIO5");
    ESP_LOGI(TAG, "LED BLUE    = GPIO12");
    ESP_LOGI(TAG, "APN         = %s", APN);
    ESP_LOGI(TAG, "MQTT broker = %s:%d", MQTT_HOST, MQTT_PORT);
    ESP_LOGI(TAG, "Topic data  = %s", MQTT_TOPIC_DATA);
    ESP_LOGI(TAG, "Topic cmd   = %s", MQTT_TOPIC_CMD);
    ESP_LOGI(TAG, "========================================");

    bg95_config_t bg95_config = {
        .uart_port = MODEM_UART_PORT,
        .tx_pin = MODEM_TX_PIN,
        .rx_pin = MODEM_RX_PIN,
        .pwrkey_pin = MODEM_PWRKEY_PIN,
        .baud_rate = MODEM_BAUD_RATE,
        .apn = APN,
        .apn_user = APN_USER,
        .apn_pass = APN_PASS,
    };

    led_rgb_config_t led_config = {
        .red_pin = LED_RED_PIN,
        .green_pin = LED_GREEN_PIN,
        .blue_pin = LED_BLUE_PIN,
        .active_level = LED_ACTIVE_LEVEL,
    };

    g_app_events = xEventGroupCreate();
    g_modem_queue = xQueueCreate(10, sizeof(modem_request_t));
    g_led_queue = xQueueCreate(10, sizeof(led_request_t));

    ESP_ERROR_CHECK(bg95_init(&bg95_config));
    ESP_ERROR_CHECK(bg95_powerkey_init());
    ESP_ERROR_CHECK(led_rgb_init(&led_config));

    led_rgb_set(LED_RGB_BLUE);

    xTaskCreate(modem_task, "modem_task", 8192, NULL, 8, NULL);
    xTaskCreate(telemetry_task, "telemetry_task", 4096, NULL, 5, NULL);
    xTaskCreate(led_task, "led_task", 3072, NULL, 6, NULL);
}
