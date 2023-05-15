#pragma once
#include <stdint.h>
typedef enum {
  BM_SERIAL_OK = 0,
  BM_SERIAL_NULL_BUFF = -1,
  BM_SERIAL_OVERFLOW = -2,
  BM_SERIAL_MISSING_CALLBACK = -3,
  BM_SERIAL_OUT_OF_MEMORY = -4,
  BM_SERIAL_TX_ERR = -5,
  BM_SERIAL_CRC_ERR = -6,
  BM_SERIAL_UNSUPPORTED_MSG = -7,
  BM_SERIAL_INVALID_TOPIC_LEN = -8,
  BM_SERIAL_INVALID_MSG_LEN = -9,

  BM_SERIAL_MISC_ERR,
} bm_serial_error_e;

bm_serial_error_e bm_serial_pub(uint64_t node_id, const char *topic, uint16_t topic_len, const uint8_t *data, uint16_t data_len);
