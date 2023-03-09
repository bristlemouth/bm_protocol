#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool bm_caps_print_from_cbor(const uint8_t *payload, uint32_t payloadSize);
bool bm_caps_get_cbor(uint8_t *payload, uint32_t *payloadSize);

#ifdef __cplusplus
}
#endif
