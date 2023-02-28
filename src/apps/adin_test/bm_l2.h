#ifndef __BM_L2_H__
#define __BM_L2_H__

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "lwip/netif.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Ethernet Header              -- 14 bytes
   IPV6 Header pre-SRC address  -- 8 bytes
   SRC IPV6 address             -- 16 bytes
   DST IPV6 address             -- 16 bytes 
*/

/* These are the indices of the payload that contain bytes 5 and 6 of the payload's src IPV6 address. 
   This is where we store the ingress port for the RX device and the egress port of the TX device. */
#define EGRESS_PORT_IDX     26
#define INGRESS_PORT_IDX    27

/* Index 38 corresponds to the start of the DST IPv6 address after skipping the ethernet header and the
   the IPv6 header pre-SRC address */
#define DST_ADDR_MSB        38

err_t bm_l2_tx(struct pbuf *p, uint8_t port_mask);
err_t bm_l2_rx(void* device_handle, uint8_t* payload, uint16_t payload_len, uint8_t port_mask);
err_t bm_l2_link_output(struct netif *netif, struct pbuf *p);
err_t bm_l2_init(struct netif *netif);

#ifdef __cplusplus
}
#endif

#endif /* __BM_L2_H__ */