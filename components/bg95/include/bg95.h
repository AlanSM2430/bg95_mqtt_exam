#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "driver/uart.h"
#include "driver/gpio.h"

typedef struct {
    uart_port_t uart_port;
    gpio_num_t tx_pin;
    gpio_num_t rx_pin;
    gpio_num_t pwrkey_pin;
    int baud_rate;

    const char *apn;
    const char *apn_user;
    const char *apn_pass;
} bg95_config_t;

esp_err_t bg95_init(const bg95_config_t *config);

esp_err_t bg95_powerkey_init(void);
void bg95_power_on(void);

bool bg95_send_cmd(const char *cmd, int timeout_ms);
bool bg95_send_cmd_expect(const char *cmd, const char *expect, int timeout_ms);
const char *bg95_last_response(void);

bool bg95_wait_at(int attempts);
bool bg95_basic_info(void);
bool bg95_check_sim(void);
bool bg95_get_rssi(int *rssi);
bool bg95_wait_registration(int timeout_seconds);
bool bg95_activate_pdp(void);
bool bg95_get_ip(char *ip_out, size_t ip_out_size);
bool bg95_network_start(char *ip_out, size_t ip_out_size);

bool bg95_mqtt_open(const char *host, int port);
bool bg95_mqtt_connect(const char *client_id);
bool bg95_mqtt_subscribe(const char *topic);
bool bg95_mqtt_publish(const char *topic, const char *payload, int qos, int retain);
bool bg95_mqtt_close(void);
bool bg95_mqtt_poll_command(char *out, size_t out_size, int timeout_ms);
