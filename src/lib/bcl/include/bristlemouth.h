#pragma once

#include <stddef.h>
#include <stdint.h>

#include "lwip/pbuf.h"
#include "lwip/tcpip.h"
#include "lwip/inet.h"

#include "serial.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bcl_rx_element_s {
    struct pbuf* buf;
    ip_addr_t src;
    //other_info_t other_info;
} bcl_rx_element_t;

void bcl_init(SerialHandle_t* hSerial);

#ifdef __cplusplus
}
#endif