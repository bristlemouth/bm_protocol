#pragma once

// TODO: This file is adapted from bm_sbc/src/transports/uart_l2/frame_codec.h.
// Differences from bm_sbc: uses the firmware's third_party cobs-c macros
// (COBS_ENCODE_DST_BUF_LEN_MAX) instead of bm_sbc's COBS_ENCODE_MAX.
// Consolidate into a shared library (e.g. bm_common) when both repos stabilize.

/// @file frame_codec.h
/// @brief UART L2 frame codec — encodes/decodes L2 Ethernet frames for
///        transport over a serial link.
///
/// Wire format:
///   [COBS-encoded payload] [0x00 delimiter]
///
/// Payload (before COBS encoding):
///   [len_hi] [len_lo] [L2 frame bytes...] [crc32c (4 bytes, big-endian)]
///
/// - Length is a 2-byte big-endian value equal to the L2 frame size.
/// - CRC-32C (Castagnoli) is computed over the length + L2 frame bytes.
/// - COBS encoding ensures no 0x00 bytes appear in the encoded payload,
///   so 0x00 can serve as an unambiguous frame delimiter.

#include "cobs.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Overhead added to each L2 frame: 2 bytes length + 4 bytes CRC-32C.
#define FRAME_CODEC_OVERHEAD 6

/// Maximum L2 frame size supported (standard Ethernet MTU + header).
#define FRAME_CODEC_MAX_L2_SIZE 1522

/// Maximum wire size: COBS overhead over full payload + 1 byte 0x00 delimiter.
/// Uses COBS_ENCODE_DST_BUF_LEN_MAX from the firmware's third_party/cobs-c/cobs.h.
#define FRAME_CODEC_MAX_WIRE_SIZE                                                              \
  (COBS_ENCODE_DST_BUF_LEN_MAX(FRAME_CODEC_MAX_L2_SIZE + FRAME_CODEC_OVERHEAD) + 1)

/// Encode an L2 frame into wire format (COBS-encoded, 0x00-terminated).
///
/// @param wire      Output buffer for the encoded frame.
/// @param wire_len  Size of the output buffer (must be >= FRAME_CODEC_MAX_WIRE_SIZE).
/// @param l2_frame  The raw L2 Ethernet frame to encode.
/// @param l2_len    Length of the L2 frame in bytes.
/// @return Total bytes written to @p wire (including the 0x00 delimiter), or 0 on error.
size_t frame_encode(uint8_t *wire, size_t wire_len, const uint8_t *l2_frame, size_t l2_len);

/// Decode a wire frame back into the original L2 frame.
///
/// @param l2_frame  Output buffer for the decoded L2 frame.
/// @param l2_len    Size of the output buffer.
/// @param wire      COBS-encoded data WITHOUT the trailing 0x00 delimiter.
/// @param wire_len  Length of the wire data.
/// @return Length of the decoded L2 frame, or 0 on error (CRC mismatch,
///         length mismatch, or COBS decode failure).
size_t frame_decode(uint8_t *l2_frame, size_t l2_len, const uint8_t *wire, size_t wire_len);

#ifdef __cplusplus
}
#endif
