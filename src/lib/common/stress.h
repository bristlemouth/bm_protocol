#pragma once

#include "lwip/netif.h"
#include "network_device.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void stress_test_init(NetworkDevice network_device, uint16_t udp_port);
void stress_start_tx(uint32_t tx_len);
void stress_stop_tx();

#ifdef __cplusplus
}
#endif
