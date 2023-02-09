/*
 * Copyright (c) 2018 Workaround GmbH.
 * Copyright (c) 2017 Intel Corporation.
 * Copyright (c) 2017 Nordic Semiconductor ASA
 * Copyright (c) 2015 Runtime Inc
 * Copyright (c) 2018 Google LLC.
 * Copyright (c) 2022 Meta
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include <stddef.h>

/**
 * @brief Compute the checksum of a buffer with polynomial 0x1021, reflecting
 *        input and output.
 *
 * This function is able to calculate any CRC that uses 0x1021 as it polynomial
 * and requires reflecting both the input and the output. It is a fast variant
 * that runs in O(n) time, where n is the length of the input buffer.
 *
 * The following checksums can, among others, be calculated by this function,
 * depending on the value provided for the initial seed and the value the final
 * calculated CRC is XORed with:
 *
 * - CRC-16/CCITT, CRC-16/CCITT-TRUE, CRC-16/KERMIT
 *   https://reveng.sourceforge.io/crc-catalogue/16.htm#crc.cat.crc-16-kermit
 *   initial seed: 0x0000, xor output: 0x0000
 *
 * - CRC-16/X-25, CRC-16/IBM-SDLC, CRC-16/ISO-HDLC
 *   https://reveng.sourceforge.io/crc-catalogue/16.htm#crc.cat.crc-16-ibm-sdlc
 *   initial seed: 0xffff, xor output: 0xffff
 *
 * @note To calculate the CRC across non-contiguous blocks use the return
 *       value from block N-1 as the seed for block N.
 *
 * See ITU-T Recommendation V.41 (November 1988).
 *
 * @param seed Value to seed the CRC with
 * @param src Input bytes for the computation
 * @param len Length of the input in bytes
 *
 * @return The computed CRC16 value (without any XOR applied to it)
 */
uint16_t crc16_ccitt(uint16_t seed, const uint8_t *src, size_t len);