#pragma once
#include <stdint.h>
#include "lwip/netif.h"
#include "lwip/ip_addr.h"

err_t bcmp_send_heartbeat(uint32_t period_s);
err_t bcmp_process_heartbeat(void *payload, const ip_addr_t *src, uint8_t dst_port);
