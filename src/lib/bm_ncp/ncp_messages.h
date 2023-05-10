#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
  BM_NCP_DEBUG = 0x00,
  BM_NCP_ACK = 0x01,

  BM_NCP_PUB = 0x02,
  BM_NCP_SUB = 0x03,
  BM_NCP_UNSUB = 0x04,
  BM_NCP_LOG = 0x05,
} ncp_message_t;
