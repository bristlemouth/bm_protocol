#ifndef __BM_L2_H__
#define __BM_L2_H__

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
/* Define for actual netdev instance count here, as some of the later code currently iterates devices
   using BM_NETDEV_MAX_DEVICES (which specifies the number of enum entries, not the actual number of devices).
   If you added a new enum entry, but didn't change the instance array, the current code would crash. */
#define BM_NETDEV_COUNT (2)

typedef enum {
  BM_NETDEV_TYPE_NONE,
  BM_NETDEV_TYPE_ADIN2111,
  BM_NETDEV_TYPE_MAX
} bm_netdev_type_t;

typedef struct bm_netdev_config_s {
  bm_netdev_type_t type;
  void *config;
} bm_netdev_config_t;

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
