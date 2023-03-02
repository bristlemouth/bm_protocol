#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "lwip/netif.h"
#include "lwip/pbuf.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BM_MIDDLEWARE_PORT 4321

void bm_middleware_init(struct netif* netif, uint16_t port);
int32_t middleware_net_tx(struct pbuf *pbuf);

#ifdef __cplusplus
}
#endif
