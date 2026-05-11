#pragma once

// TODO: This file is duplicated from bm_sbc/src/transports/uart_l2/crc32c.h.
// Consolidate into a shared library (e.g. bm_common) when both repos stabilize.

/// @file crc32c.h
/// @brief CRC-32C (Castagnoli) for UART frame integrity.
///
/// Uses polynomial 0x1EDC6F41 (bit-reflected: 0x82F63B78).
/// Provides better burst-error detection than CRC-32 IEEE for the
/// types of bit errors common on embedded serial links.

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Compute CRC-32C over @p len bytes starting at @p data.
/// @return The CRC-32C value.
uint32_t crc32c(const uint8_t *data, size_t len);

/// Incrementally update a running CRC-32C with additional data.
/// @param crc  Previous CRC value (pass 0xFFFFFFFF for the first call).
/// @param data Pointer to new data.
/// @param len  Length of new data.
/// @return Updated CRC (call crc32c_finalize() after all data is fed).
uint32_t crc32c_update(uint32_t crc, const uint8_t *data, size_t len);

/// Finalize a running CRC-32C (XOR with 0xFFFFFFFF).
static inline uint32_t crc32c_finalize(uint32_t crc) { return crc ^ 0xFFFFFFFF; }

#ifdef __cplusplus
}
#endif
