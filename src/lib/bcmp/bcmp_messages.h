#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
  uint32_t count;
} bcmpHeartbeat_t;


typedef enum {
  BCMP_ACK = 0x00,
  BCMP_HEARTBEAT = 0x01,
} bcmpMessaegType_t;

err_t bcmp_send_heartbeat(uint32_t count);
err_t bcmp_process_heartbeat(void *payload, const ip_addr_t *src);
