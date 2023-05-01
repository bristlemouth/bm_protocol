#pragma once
#include <stdint.h>

typedef struct __attribute__((__packed__)) bm_dfu_img_info_s {
    uint32_t image_size;
    uint16_t chunk_size;
    uint16_t crc16;
    uint8_t major_ver;
    uint8_t minor_ver;
    uint32_t filter_key; // Will be used later for image rejection.
} bm_dfu_img_info_t;

typedef struct __attribute__((__packed__)) bm_dfu_frame_header_s {
    uint8_t frame_type;
} bm_dfu_frame_header_t;

typedef struct __attribute__((__packed__)) bm_dfu_frame_s {
    bm_dfu_frame_header_t header;
    uint8_t payload[0];
} bm_dfu_frame_t;

typedef struct __attribute__((__packed__)) bm_dfu_event_init_success_s {
    uint8_t reserved;
}bm_dfu_event_init_success_t;

typedef struct __attribute__((__packed__)) bm_dfu_address_s {
    uint64_t src_node_id;
    uint64_t dst_node_id;
} bm_dfu_event_address_t;

typedef struct __attribute__((__packed__)) bm_dfu_event_chunk_request_s {
    bm_dfu_event_address_t addresses;
    uint16_t seq_num;
} bm_dfu_event_chunk_request_t;

typedef struct __attribute__((__packed__)) bm_dfu_event_image_chunk_s {
    bm_dfu_event_address_t addresses;
    uint16_t payload_length;
    uint8_t payload_buf[0];
} bm_dfu_event_image_chunk_t;

typedef struct __attribute__((__packed__)) bm_dfu_result_s {
    bm_dfu_event_address_t addresses;
    uint8_t success;
    uint8_t err_code;
} bm_dfu_event_result_t;

typedef struct __attribute__((__packed__)) bm_dfu_event_img_info_s {
    bm_dfu_event_address_t addresses;
    bm_dfu_img_info_t img_info;
} bm_dfu_event_img_info_t;
