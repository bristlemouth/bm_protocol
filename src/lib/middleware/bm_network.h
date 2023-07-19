#pragma once
#include <stdint.h>

// FIXME: Add to shared message definition.
typedef enum {
    BM_NETWORK_TYPE_CELLULAR_IRI_FALLBACK = (1 << 0),
    BM_NETWORK_TYPE_CELLULAR_ONLY = (1 << 1),
} bm_serial_network_type_e;

bool spotter_tx_data(const void* data, uint16_t data_len, bm_serial_network_type_e type);
