#pragma once

#include <stdbool.h>
#include <stdint.h>

void bcl_power_callback(bool on);
bool bcl_init(void);
void bcl_set_network_interrupt(bool enable);
const char *bcl_get_ip_str(uint8_t ip);
