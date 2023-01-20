#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "adi_mac.h"
#include "adin2111.h"
#include "lwip/netif.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ADIN_PORT_MASK      0x03
#define QUEUE_NUM_ENTRIES   8

/* Extra 4 bytes for FCS and 2 bytes for the frame header */
#define MAX_FRAME_BUF_SIZE  (MAX_FRAME_SIZE + 4 + 2)

typedef struct queue_entry_t{
    adin2111_Port_e port;
    adi_eth_BufDesc_t *pBufDesc;
    adin2111_DeviceHandle_t dev;
    bool sent;
} queue_entry_t;

typedef struct queue_t{
    uint32_t head;
    uint32_t tail;
    bool full;
    queue_entry_t entries[QUEUE_NUM_ENTRIES];
} queue_t;

typedef struct rx_port_info_t {
    adin2111_Port_e port;
    adi_mac_Device_t* dev;
} rx_info_t;

adin2111_DeviceHandle_t adin2111_hw_init(void);
err_t adin2111_tx(void* dev_handle, uint8_t* buf, uint16_t buf_len, uint8_t port_mask);
int adin2111_hw_start(adin2111_DeviceHandle_t* dev);
int adin2111_hw_stop(adin2111_DeviceHandle_t* dev);

#ifdef __cplusplus
}
#endif