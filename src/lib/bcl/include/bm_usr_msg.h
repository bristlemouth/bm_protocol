#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include "zcbor_encode.h"

typedef struct bm_usr_msg_hdr_t {
    uint8_t     encoding;
    uint8_t     set_id;
    uint16_t    id;
} bm_usr_msg_hdr_t;

/* Encoding Type */
enum {
    BM_ENCODING_CBOR,
    BM_ENCODING_NONE,
};

/* Set ID */
enum {
    BM_SET_ROS,
    BM_SET_DEFAULT,
};