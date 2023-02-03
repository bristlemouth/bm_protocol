#ifndef __BM_L2_H__
#define __BM_L2_H__

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "lwip/netif.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define EGRESS_PORT_IDX     32
#define INGRESS_PORT_IDX    33

err_t bm_l2_tx(struct pbuf *p, uint8_t port_mask);
err_t bm_l2_rx(void* device_handle, uint8_t* payload, uint16_t payload_len, uint8_t port_mask);
err_t bm_l2_link_output(struct netif *netif, struct pbuf *p);
err_t bm_l2_init(struct netif *netif);

#ifdef __cplusplus
}
#endif

#endif /* __BM_L2_H__ */