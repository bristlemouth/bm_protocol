#pragma once

#include <stdbool.h>
#include <stdint.h>

void bcl_power_callback(bool on);
void bcl_init(void);
const char *bcl_get_ip_str(uint8_t ip);
