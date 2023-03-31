#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "adi_mac.h"
#include "adin2111.h"
#include "lwip/netif.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ADIN2111_PORT_MASK  0x03
#define QUEUE_NUM_ENTRIES   8

/* Extra 4 bytes for FCS and 2 bytes for the frame header */
#define MAX_FRAME_BUF_SIZE  (MAX_FRAME_SIZE + 4 + 2)

typedef int8_t (*adin_rx_callback_t)(void* device_handle, uint8_t* payload, uint16_t payload_len, uint8_t port_mask);

adi_eth_Result_e adin2111_hw_init(adin2111_DeviceHandle_t hDevice, adin_rx_callback_t rx_callback);
err_t adin2111_tx(adin2111_DeviceHandle_t hDevice, uint8_t* buf, uint16_t buf_len, uint8_t port_mask, uint8_t port_offset);
int adin2111_hw_start(adin2111_DeviceHandle_t* dev);
int adin2111_hw_stop(adin2111_DeviceHandle_t* dev);

#ifdef __cplusplus
}
#endif
