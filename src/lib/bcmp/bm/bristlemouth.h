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

void bcl_init(void);
const char *bcl_get_ip_str(uint8_t ip);

#ifdef __cplusplus
}
#endif
