/*
 * Generated using zcbor version 0.6.99
 * https://github.com/NordicSemiconductor/zcbor
 * Generated with a --default-max-qty of 3
 */

#ifndef BM_ZCBOR_DECODE_H__
#define BM_ZCBOR_DECODE_H__

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include "bm_zcbor_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#if DEFAULT_MAX_QTY != 3
#error "The type file was generated with a different default_max_qty than this file"
#endif


int cbor_decode_bm_Discover_Neighbors(
		const uint8_t *payload, size_t payload_len,
		struct bm_Discover_Neighbors *result,
		size_t *payload_len_out);


int cbor_decode_bm_Neighbor_Info(
		const uint8_t *payload, size_t payload_len,
		struct bm_Neighbor_Info *result,
		size_t *payload_len_out);


int cbor_decode_bm_Time(
		const uint8_t *payload, size_t payload_len,
		struct bm_Time *result,
		size_t *payload_len_out);


int cbor_decode_bm_Table_Response(
		const uint8_t *payload, size_t payload_len,
		struct bm_Table_Response *result,
		size_t *payload_len_out);


int cbor_decode_bm_Duration(
		const uint8_t *payload, size_t payload_len,
		struct bm_Duration *result,
		size_t *payload_len_out);


int cbor_decode_bm_Ack(
		const uint8_t *payload, size_t payload_len,
		struct bm_Ack *result,
		size_t *payload_len_out);


int cbor_decode_bm_Header(
		const uint8_t *payload, size_t payload_len,
		struct bm_Header *result,
		size_t *payload_len_out);


int cbor_decode_bm_Heartbeat(
		const uint8_t *payload, size_t payload_len,
		struct bm_Heartbeat *result,
		size_t *payload_len_out);


int cbor_decode_bm_Request_Table(
		const uint8_t *payload, size_t payload_len,
		struct bm_Request_Table *result,
		size_t *payload_len_out);


int cbor_decode_bm_Temperature(
		const uint8_t *payload, size_t payload_len,
		struct bm_Temperature *result,
		size_t *payload_len_out);


#ifdef __cplusplus
}
#endif

#endif /* BM_ZCBOR_DECODE_H__ */
