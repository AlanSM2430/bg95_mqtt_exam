#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "bg95.h"

#define BG95_RESPONSE_SIZE 4096

extern bg95_config_t g_bg95_cfg;
extern char g_bg95_response[BG95_RESPONSE_SIZE];

int bg95_read_raw(char *out, size_t out_size, int timeout_ms);
