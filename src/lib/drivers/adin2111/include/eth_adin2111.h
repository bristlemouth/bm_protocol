#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "adin2111.h"
#include "lwip/netif.h"

#ifdef __cplusplus
extern "C" {
#endif

// Maximum frame size for the ADIN2111
#define ETH_MAX_FRAME_SIZE (MAX_FRAME_SIZE)

#define ADIN2111_PORT_MASK  0x03
#define QUEUE_NUM_ENTRIES   8

typedef struct {
  adi_phy_MseLinkQuality_t mse_link_quality;
  adi_phy_FrameChkErrorCounters_t frame_check_err_counters;
  uint16_t frame_check_rx_err_cnt;
} adin_port_stats_t;

typedef int8_t (*adin_rx_callback_t)(void* device_handle, uint8_t* payload, uint16_t payload_len, uint8_t port_mask);
typedef void (*adin_link_change_callback_t)(void* device_handle, uint8_t port, bool state);
typedef void (*adin2111_port_stats_callback_t)(adin2111_DeviceHandle_t device_handle, adin2111_Port_e port, adin_port_stats_t *stats, void* args);

adi_eth_Result_e adin2111_hw_init(adin2111_DeviceHandle_t hDevice, adin_rx_callback_t rx_callback, adin_link_change_callback_t link_change_callback);
err_t adin2111_tx(adin2111_DeviceHandle_t hDevice, uint8_t* buf, uint16_t buf_len, uint8_t port_mask, uint8_t port_offset);
int adin2111_hw_start(adin2111_DeviceHandle_t dev);
int adin2111_hw_stop(adin2111_DeviceHandle_t dev);
bool adin2111_get_port_stats(adin2111_DeviceHandle_t dev, adin2111_Port_e port, adin2111_port_stats_callback_t cb, void* args);

#ifdef __cplusplus
}
#endif
