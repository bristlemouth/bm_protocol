#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "adi_mac.h"
#include "adin2111.h"
#include "lwip/netif.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ADIN_MAC_ADDR_0 (0x00)
#define ADIN_MAC_ADDR_1 (0x0A)
#define ADIN_MAC_ADDR_2 (0x83)

#define QUEUE_NUM_ENTRIES (8)

/* Extra 4 bytes for FCS and 2 bytes for the frame header */
#define MAX_FRAME_BUF_SIZE  (MAX_FRAME_SIZE + 4 + 2)

typedef struct {
    adin2111_Port_e port;
    adi_eth_BufDesc_t *pBufDesc;
    bool sent;
} queue_entry_t;

typedef struct {
    uint32_t head;
    uint32_t tail;
    bool full;
    queue_entry_t entries[QUEUE_NUM_ENTRIES];
} queue_t;

err_t eth_adin2111_init(struct netif *netif);
int adin2111_hw_start(adin2111_DeviceHandle_t* dev);
int adin2111_hw_stop(adin2111_DeviceHandle_t* dev);

#ifdef __cplusplus
}
#endif