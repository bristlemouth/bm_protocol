#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef uint64_t bm_time_t;

typedef struct {
  // Time since unit powered up
  bm_time_t uptime_us;

  // Time between heartbeat messages
  bm_time_t period_us;
} bcmpHeartbeat_t;

typedef enum {
  BCMP_ACK = 0x00,
  BCMP_HEARTBEAT = 0x01,

  // DFU
  BCMP_DFU_START = 0xD0,
  BCMP_DFU_PAYLOAD_REQ = 0xD1,
  BCMP_DFU_PAYLOAD = 0xD2,
  BCMP_DFU_END = 0xD3,
  BCMP_DFU_ACK = 0xD4,
  BCMP_DFU_ABORT = 0xD5,
  BCMP_DFU_HEARTBEAT = 0xD6,
  BCMP_DFU_BEGIN_HOST = 0xD7,
} bcmpMessaegType_t;

err_t bcmp_send_heartbeat(uint32_t period_s);
err_t bcmp_process_heartbeat(void *payload, const ip_addr_t *src, uint8_t dst_port);
