#pragma once

#include <stdint.h>
#include "lwip/netif.h"

#ifdef __cplusplus
extern "C" {
#endif

void stress_test_init(struct netif* netif, uint16_t port);
void stress_start_tx(uint32_t tx_len);
void stress_stop_tx();

#ifdef __cplusplus
}
#endif
