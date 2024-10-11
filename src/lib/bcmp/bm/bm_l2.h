#ifndef __BM_L2_H__
#define __BM_L2_H__

#include "bm_config.h"
#include "lwip/netif.h"
#include "util.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* We store the egress port for the RX device and the ingress port of the TX device
   in bytes 5 (index 4) and 6 (index 5) of the SRC IPv6 address. */
#define EGRESS_PORT_IDX 4
#define INGRESS_PORT_IDX 5

typedef void (*bm_l2_link_change_cb_t)(uint8_t port, bool state);

BmErr bm_l2_link_output(void *buf, uint32_t length);
BmErr bm_l2_init(bm_l2_link_change_cb_t link_change_cb);
void bm_l2_netif_set_power(void *dev, bool on);

bool bm_l2_get_device_handle(uint8_t dev_idx, void **device_handle, bm_netdev_type_t *type,
                             uint32_t *start_port_idx);
uint8_t bm_l2_get_num_ports(void);
bool bm_l2_get_port_state(uint8_t port);

#ifdef __cplusplus
}
#endif

#endif /* __BM_L2_H__ */
