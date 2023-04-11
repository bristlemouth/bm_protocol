#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "lwip/netif.h"
#include "lwip/ip_addr.h"

#include "bcmp_messages.h"
#include "bm_util.h"

#ifdef __cplusplus
extern "C" {
#endif

#define IP_PROTO_BCMP (0xBC)

typedef struct {
  uint16_t type;
  uint16_t checksum;
  uint8_t flags;
  uint8_t rsvd;
  uint8_t payload[0];
} __attribute__((packed)) bcmpHeader_t;

void bcmp_init(struct netif* netif);
err_t bcmp_tx(const ip_addr_t *dst, bcmpMessaegType_t type, uint8_t *buff, uint16_t len);
err_t bcmp_send_heartbeat(uint32_t count);

#ifdef __cplusplus
}
#endif
