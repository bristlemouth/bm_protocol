#pragma once
#include <stdint.h>
extern "C" {
#include "util.h"
}

bool bcmp_time_set_time(uint64_t target_node_id, uint64_t utc_us);
bool bcmp_time_get_time(uint64_t target_node_id);
BmErr time_init(void);
