#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
  BCMP_CAPS_ALL = 0,
  BCMP_MSG_LIST = 1,
} bcmp_cap_id_e;

typedef struct {
  // Capability ID (see bcmp_cap_id)
  uint16_t id;

  // Length of value field
  uint16_t len;

  // Capability structure data, to be intepreted based on id
  uint8_t value[0];
} __attribute__((packed)) bcmp_cap_tlv_t;

typedef struct {
  // Length of message ID list
  uint16_t len;

  // List of message IDs (see Type fields of top-level BMCP message list)
  uint16_t msg_ids[0];
} __attribute__((packed)) bcmp_msg_list_t;
