/*
 * Generated using zcbor version 0.6.99
 * https://github.com/NordicSemiconductor/zcbor
 * Generated with a --default-max-qty of 3
 */

#ifndef BM_ZCBOR_ENCODE_H__
#define BM_ZCBOR_ENCODE_H__

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


int cbor_encode_bm_Discover_Neighbors(
		uint8_t *payload, size_t payload_len,
		const struct bm_Discover_Neighbors *input,
		size_t *payload_len_out);


int cbor_encode_bm_Neighbor_Info(
		uint8_t *payload, size_t payload_len,
		const struct bm_Neighbor_Info *input,
		size_t *payload_len_out);


int cbor_encode_bm_Time(
		uint8_t *payload, size_t payload_len,
		const struct bm_Time *input,
		size_t *payload_len_out);


int cbor_encode_bm_Table_Response(
		uint8_t *payload, size_t payload_len,
		const struct bm_Table_Response *input,
		size_t *payload_len_out);


int cbor_encode_bm_Duration(
		uint8_t *payload, size_t payload_len,
		const struct bm_Duration *input,
		size_t *payload_len_out);


int cbor_encode_bm_Ack(
		uint8_t *payload, size_t payload_len,
		const struct bm_Ack *input,
		size_t *payload_len_out);


int cbor_encode_bm_Header(
		uint8_t *payload, size_t payload_len,
		const struct bm_Header *input,
		size_t *payload_len_out);


int cbor_encode_bm_Heartbeat(
		uint8_t *payload, size_t payload_len,
		const struct bm_Heartbeat *input,
		size_t *payload_len_out);


int cbor_encode_bm_Request_Table(
		uint8_t *payload, size_t payload_len,
		const struct bm_Request_Table *input,
		size_t *payload_len_out);


int cbor_encode_bm_Temperature(
		uint8_t *payload, size_t payload_len,
		const struct bm_Temperature *input,
		size_t *payload_len_out);


#ifdef __cplusplus
}
#endif

#endif /* BM_ZCBOR_ENCODE_H__ */
