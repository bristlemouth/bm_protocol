#pragma once

#include "lwip/ip.h"
#include "lwip/ip_addr.h"
#include <stdint.h>
extern "C" {
#include "util.h"
}

void bcmp_expect_info_from_node_id(uint64_t node_id);
err_t bcmp_request_info(uint64_t target_node_id, const ip_addr_t *addr,
                        void (*cb)(void *) = NULL);
BmErr bcmp_process_info_init(void);
