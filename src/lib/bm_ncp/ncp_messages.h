#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
  uint16_t crc16;
  uint8_t type;
  uint8_t flags;
  uint8_t payload[0];
} __attribute__ ((packed)) bcmp_ncp_packet_t;

typedef enum {
  BCMP_NCP_DEBUG = 0x00,
  BCMP_NCP_ACK = 0x01,

  BCMP_NCP_PUB = 0x02,
  BCMP_NCP_SUB = 0x03,
  BCMP_NCP_UNSUB = 0x04,
  BCMP_NCP_LOG = 0x05,
} bcmp_ncp_message_t;
