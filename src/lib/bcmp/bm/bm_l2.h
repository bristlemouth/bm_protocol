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
#define egress_port_idx 4
#define ingress_port_idx 5

typedef void (*BmL2ModuleLinkChangeCb)(uint8_t port, bool state);
typedef void (*BmL2DeviceLinkChangeCb)(void *device_handle, uint8_t port, bool state);
typedef BmErr (*BmL2TxCb)(void *device_handle, uint8_t *buff, uint16_t size, uint8_t port_mask,
                          uint8_t port_offset);
typedef BmErr (*BmL2RxCb)(void *device_handle, uint8_t *payload, uint16_t size,
                          uint8_t port_mask);
typedef BmErr (*BmL2InitCb)(void *device_handle, BmL2RxCb rx_cb,
                            BmL2DeviceLinkChangeCb link_change_cb, uint8_t port_mask);
typedef BmErr (*BmL2PowerDownCb)(const void *devHandle, bool on, uint8_t port_mask);

typedef struct {
  void *device_handle;
  uint8_t port_mask;
  uint8_t num_ports;
  BmL2InitCb init_cb;
  BmL2PowerDownCb power_cb;
  BmL2TxCb tx_cb;
} BmNetDevCfg;

BmErr bm_l2_link_output(void *buf, uint32_t length);
BmErr bm_l2_init(BmL2ModuleLinkChangeCb cb, const BmNetDevCfg *cfg, uint8_t count);
void bm_l2_netif_set_power(void *dev, bool on);

bool bm_l2_get_device_handle(uint8_t dev_idx, void **device_handle, uint32_t *start_port_idx);
uint8_t bm_l2_get_num_ports(void);
uint8_t bm_l2_get_num_devices(void);
bool bm_l2_get_port_state(uint8_t port);

#ifdef __cplusplus
}
#endif

#endif /* __BM_L2_H__ */
