#pragma once

#include <stdint.h>
#include "lwip/ip_addr.h"

err_t bcmp_send_heartbeat(uint32_t lease_duration_s);
err_t bcmp_process_heartbeat(bcmp_heartbeat_t *heartbeat, const ip_addr_t *src, uint8_t dst_port);
