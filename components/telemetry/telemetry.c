#include <stdio.h>
#include <stdint.h>

#include "esp_timer.h"
#include "esp_system.h"

#include "telemetry.h"

void telemetry_build_json(
    char *out,
    size_t out_size,
    const char *device_id,
    int rssi,
    const char *ip,
    const char *led_state
)
{
    uint64_t uptime_s = esp_timer_get_time() / 1000000ULL;
    uint32_t free_heap = esp_get_free_heap_size();

    snprintf(
        out,
        out_size,
        "{"
            "\"device\":\"%s\","
            "\"uptime\":%llu,"
            "\"rssi\":%d,"
            "\"ip\":\"%s\","
            "\"led\":\"%s\","
            "\"free_heap\":%lu,"
            "\"saludo\":\"hola desde ESP32-C6 usando BG95-M3 por LTE-M\""
        "}",
        device_id,
        uptime_s,
        rssi,
        ip ? ip : "",
        led_state ? led_state : "unknown",
        (unsigned long)free_heap
    );
}
