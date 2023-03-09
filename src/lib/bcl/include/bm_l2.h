#ifndef __BM_L2_H__
#define __BM_L2_H__

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "lwip/netif.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* We store the egress port for the RX device and the ingress port of the TX device
   in bytes 5 (index 4) and 6 (index 5) of the SRC IPv6 address. */
#define EGRESS_PORT_IDX     4
#define INGRESS_PORT_IDX    5

/* IPv6 is stored in uint32_t array of size 4 */
enum {
    IPV6_ADDR_DWORD_0,
    IPV6_ADDR_DWORD_1,
    IPV6_ADDR_DWORD_2,
    IPV6_ADDR_DWORD_3
};

err_t bm_l2_tx(struct pbuf *p, uint8_t port_mask);
err_t bm_l2_rx(void* device_handle, uint8_t* payload, uint16_t payload_len, uint8_t port_mask);
err_t bm_l2_link_output(struct netif *netif, struct pbuf *p);
err_t bm_l2_init(struct netif *netif);

#define ADD_EGRESS_PORT(addr, port) (addr[sizeof(struct eth_hdr) + offsetof(struct ip6_hdr, src) + EGRESS_PORT_IDX] = port)
#define ADD_INGRESS_PORT(addr, port) (addr[sizeof(struct eth_hdr) + offsetof(struct ip6_hdr, src) + INGRESS_PORT_IDX] = port)
#define IS_GLOBAL_MULTICAST(addr) ( ( (uint8_t *) addr )[sizeof(struct eth_hdr) + offsetof(struct ip6_hdr, dest)] == 0xFFU && \
                                    ( (uint8_t *) addr )[sizeof(struct eth_hdr) + offsetof(struct ip6_hdr, dest) + 1] == 0x03U )

#ifdef __cplusplus
}
#endif

#endif /* __BM_L2_H__ */
