#pragma once

extern "C" {
#include "util.h"
}
#include <stdint.h>

BmErr bcmp_send_heartbeat(uint32_t lease_duration_s);
BmErr heartbeat_init(void);
