#pragma once

#include <stddef.h>
#include <stdint.h>

void telemetry_build_json(
    char *out,
    size_t out_size,
    const char *device_id,
    int rssi,
    const char *ip,
    const char *led_state
);
