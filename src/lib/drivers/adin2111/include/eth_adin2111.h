#pragma once

#include "adin2111.h"
#include "l2.h"
#include "lwip/netif.h"
#include "util.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Maximum frame size for the ADIN2111
#define ETH_MAX_FRAME_SIZE (MAX_FRAME_SIZE)
#define QUEUE_NUM_ENTRIES 8

typedef struct {
  adi_phy_MseLinkQuality_t mse_link_quality;
  adi_phy_FrameChkErrorCounters_t frame_check_err_counters;
  uint16_t frame_check_rx_err_cnt;
} adin_port_stats_t;

typedef void (*adin2111_port_stats_callback_t)(adin2111_DeviceHandle_t device_handle,
                                               adin2111_Port_e port, adin_port_stats_t *stats,
                                               void *args);

BmErr adin2111_hw_init(void *device, BmL2RxCb rx_callback,
                       BmL2DeviceLinkChangeCb link_change_callback, uint8_t enabled_port_mask);
BmErr adin2111_tx(void *device, uint8_t *buf, uint16_t buf_len, uint8_t port_mask,
                  uint8_t port_offset);
int adin2111_hw_start(adin2111_DeviceHandle_t dev, uint8_t port_mask);
int adin2111_hw_stop(adin2111_DeviceHandle_t dev, uint8_t port_mask);
bool adin2111_get_port_stats(adin2111_DeviceHandle_t dev, adin2111_Port_e port,
                             adin2111_port_stats_callback_t cb, void *args);
BmErr adin2111_power_cb(const void *devHandle, bool on, uint8_t port_mask);
#ifdef __cplusplus
}
#endif
