#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "lwip/netif.h"
#include "lwip/pbuf.h"

#ifdef __cplusplus
extern "C" {
#endif

int32_t bm_middleware_local_pub(struct pbuf *pbuf);
void bm_middleware_init(struct netif* netif, uint16_t port);
int32_t middleware_net_tx(struct pbuf *pbuf);

#ifdef __cplusplus
}
#endif
