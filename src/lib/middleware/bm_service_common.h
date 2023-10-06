#pragma once

#define BM_SERVICE_REQ_STR "/req"
#define BM_SERVICE_REP_STR "/rep"

#define MAX_BM_SERVICE_DATA_SIZE (1024)

typedef struct bm_service_request_data_header {
    uint32_t id;
    uint32_t data_size;
    uint8_t data[0];
} __attribute__ ((packed)) bm_service_request_data_header_s;

typedef struct bm_service_reply_data_header {
    uint64_t target_node_id; // Note: Not needed when we have resource-based routing.
    uint32_t id;
    uint32_t data_size;
    uint8_t data[0];
} __attribute__ ((packed)) bm_service_reply_data_header_s;
