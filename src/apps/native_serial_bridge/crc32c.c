// TODO: This file is duplicated from bm_sbc/src/transports/uart_l2/crc32c.c.
// Consolidate into a shared library (e.g. bm_common) when both repos stabilize.

#include "crc32c.h"

/// Nibble-based CRC-32C lookup table (bit-reflected polynomial 0x82F63B78).
/// Same 4-bit-at-a-time approach used by bm_core's crc32_ieee, but with the
/// Castagnoli polynomial for better burst-error detection on serial links.
static const uint32_t table[16] = {
    0x00000000U, 0x105EC76FU, 0x20BD8EDEU, 0x30E349B1U, 0x417B1DBCU, 0x5125DAD3U,
    0x61C69362U, 0x7198540DU, 0x82F63B78U, 0x92A8FC17U, 0xA24BB5A6U, 0xB21572C9U,
    0xC38D26C4U, 0xD3D3E1ABU, 0xE330A81AU, 0xF36E6F75U,
};

uint32_t crc32c_update(uint32_t crc, const uint8_t *data, size_t len) {
  for (size_t i = 0; i < len; i++) {
    crc = (crc >> 4) ^ table[(crc ^ data[i]) & 0x0F];
    crc = (crc >> 4) ^ table[(crc ^ ((uint32_t)data[i] >> 4)) & 0x0F];
  }
  return crc;
}

uint32_t crc32c(const uint8_t *data, size_t len) {
  return crc32c_finalize(crc32c_update(0xFFFFFFFF, data, len));
}
