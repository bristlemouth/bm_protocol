#pragma once

#include <stddef.h>
#include <stdint.h>
extern "C" {
#include "util.h"
}

void bcmp_expect_info_from_node_id(uint64_t node_id);
BmErr bcmp_request_info(uint64_t target_node_id, const void *addr, void (*cb)(void *) = NULL);
BmErr bcmp_process_info_init(void);
