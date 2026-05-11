// TODO: This file is adapted from bm_sbc/src/transports/uart_l2/frame_codec.c.
// Differences from bm_sbc: the firmware's third_party/cobs-c library returns
// cobs_encode_result / cobs_decode_result structs (with .status and .out_len
// fields) instead of a bare size_t. All cobs_encode / cobs_decode call sites
// are updated accordingly.
// Consolidate into a shared library (e.g. bm_common) when both repos stabilize.

#include "frame_codec.h"
#include "cobs.h"
#include "crc32c.h"
#include <string.h>

size_t frame_encode(uint8_t *wire, size_t wire_len, const uint8_t *l2_frame, size_t l2_len) {
  if (!wire || !l2_frame || l2_len == 0 || l2_len > FRAME_CODEC_MAX_L2_SIZE) {
    return 0;
  }

  // Build the pre-COBS payload: [len_hi, len_lo, l2_frame..., crc32c (4 bytes)]
  const size_t payload_len = 2 + l2_len + 4;
  uint8_t payload[2 + FRAME_CODEC_MAX_L2_SIZE + 4];

  // 2-byte big-endian length of L2 frame.
  payload[0] = (uint8_t)(l2_len >> 8);
  payload[1] = (uint8_t)(l2_len & 0xFF);

  // L2 frame data.
  memcpy(&payload[2], l2_frame, l2_len);

  // CRC-32C over length + L2 frame bytes.
  const size_t crc_input_len = 2 + l2_len;
  uint32_t crc = crc32c(payload, crc_input_len);
  payload[crc_input_len + 0] = (uint8_t)(crc >> 24);
  payload[crc_input_len + 1] = (uint8_t)(crc >> 16);
  payload[crc_input_len + 2] = (uint8_t)(crc >> 8);
  payload[crc_input_len + 3] = (uint8_t)(crc & 0xFF);

  // Need room for COBS-encoded data + 1 byte for the 0x00 delimiter.
  if (wire_len < COBS_ENCODE_DST_BUF_LEN_MAX(payload_len) + 1) {
    return 0;
  }

  // COBS-encode the payload.
  // Firmware cobs-c API returns a cobs_encode_result struct, unlike bm_sbc's
  // minimal COBS which returns a bare size_t.
  cobs_encode_result enc = cobs_encode(wire, wire_len - 1, payload, payload_len);
  if (enc.status != COBS_ENCODE_OK) {
    return 0;
  }

  // Append 0x00 delimiter.
  wire[enc.out_len] = 0x00;
  return enc.out_len + 1;
}

size_t frame_decode(uint8_t *l2_frame, size_t l2_len, const uint8_t *wire, size_t wire_len) {
  if (!l2_frame || !wire || wire_len == 0) {
    return 0;
  }

  // COBS-decode into a temporary buffer.
  // Firmware cobs-c API returns a cobs_decode_result struct, unlike bm_sbc's
  // minimal COBS which returns a bare size_t.
  uint8_t decoded[FRAME_CODEC_MAX_L2_SIZE + FRAME_CODEC_OVERHEAD];
  cobs_decode_result dec = cobs_decode(decoded, sizeof(decoded), wire, wire_len);
  if (dec.status != COBS_DECODE_OK) {
    return 0;
  }

  if (dec.out_len < FRAME_CODEC_OVERHEAD) {
    // Too short to contain the 2-byte length field and 4-byte CRC.
    return 0;
  }

  // Extract the 2-byte big-endian length field.
  uint16_t frame_len = ((uint16_t)decoded[0] << 8) | decoded[1];

  // Validate the length.
  if (frame_len == 0 || frame_len > FRAME_CODEC_MAX_L2_SIZE) {
    return 0;
  }
  if (dec.out_len != (size_t)frame_len + FRAME_CODEC_OVERHEAD) {
    return 0;
  }

  // Verify CRC-32C over length + L2 frame bytes.
  const size_t crc_input_len = 2 + frame_len;
  uint32_t crc_computed = crc32c(decoded, crc_input_len);
  uint32_t crc_received = ((uint32_t)decoded[crc_input_len + 0] << 24) |
                          ((uint32_t)decoded[crc_input_len + 1] << 16) |
                          ((uint32_t)decoded[crc_input_len + 2] << 8) |
                          ((uint32_t)decoded[crc_input_len + 3]);
  if (crc_computed != crc_received) {
    return 0;
  }

  // Copy L2 frame to output buffer.
  if (frame_len > l2_len) {
    return 0;
  }
  memcpy(l2_frame, &decoded[2], frame_len);
  return frame_len;
}
