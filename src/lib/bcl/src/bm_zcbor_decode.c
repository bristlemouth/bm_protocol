/*
 * Generated using zcbor version 0.6.99
 * https://github.com/NordicSemiconductor/zcbor
 * Generated with a --default-max-qty of 3
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include "zcbor_decode.h"
#include "bm_zcbor_decode.h"

#if DEFAULT_MAX_QTY != 3
#error "The type file was generated with a different default_max_qty than this file"
#endif

static bool decode_repeated_bm_Discover_Neighbors_src_ipv6_addr_uint(zcbor_state_t *state, uint32_t *result);
static bool decode_repeated_bm_Neighbor_Info_ipv6_addr_uint(zcbor_state_t *state, uint32_t *result);
static bool decode_repeated_bm_Ack_src_ipv6_addr_uint(zcbor_state_t *state, uint32_t *result);
static bool decode_repeated_bm_Ack_dst_ipv6_addr_uint(zcbor_state_t *state, uint32_t *result);
static bool decode_repeated_bm_Heartbeat_src_ipv6_addr_uint(zcbor_state_t *state, uint32_t *result);
static bool decode_repeated_bm_Request_Table_src_ipv6_addr_uint(zcbor_state_t *state, uint32_t *result);
static bool decode_repeated_bm_Table_Response_src_ipv6_addr_uint(zcbor_state_t *state, uint32_t *result);
static bool decode_repeated_bm_Table_Response_dst_ipv6_addr_uint(zcbor_state_t *state, uint32_t *result);
static bool decode_bm_Temperature(zcbor_state_t *state, struct bm_Temperature *result);
static bool decode_bm_Request_Table(zcbor_state_t *state, struct bm_Request_Table *result);
static bool decode_bm_Heartbeat(zcbor_state_t *state, struct bm_Heartbeat *result);
static bool decode_bm_Header(zcbor_state_t *state, struct bm_Header *result);
static bool decode_bm_Ack(zcbor_state_t *state, struct bm_Ack *result);
static bool decode_bm_Duration(zcbor_state_t *state, struct bm_Duration *result);
static bool decode_bm_Table_Response(zcbor_state_t *state, struct bm_Table_Response *result);
static bool decode_bm_Time(zcbor_state_t *state, struct bm_Time *result);
static bool decode_bm_Neighbor_Info(zcbor_state_t *state, struct bm_Neighbor_Info *result);
static bool decode_bm_Discover_Neighbors(zcbor_state_t *state, struct bm_Discover_Neighbors *result);


static bool decode_repeated_bm_Discover_Neighbors_src_ipv6_addr_uint(
		zcbor_state_t *state, uint32_t *result)
{
	zcbor_print("%s\r\n", __func__);

	bool tmp_result = (((zcbor_uint32_decode(state, (&(*result))))
	&& ((((*result) <= UINT32_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))));

	if (!tmp_result)
		zcbor_trace();

	return tmp_result;
}

static bool decode_repeated_bm_Neighbor_Info_ipv6_addr_uint(
		zcbor_state_t *state, uint32_t *result)
{
	zcbor_print("%s\r\n", __func__);

	bool tmp_result = (((zcbor_uint32_decode(state, (&(*result))))
	&& ((((*result) <= UINT32_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))));

	if (!tmp_result)
		zcbor_trace();

	return tmp_result;
}

static bool decode_repeated_bm_Ack_src_ipv6_addr_uint(
		zcbor_state_t *state, uint32_t *result)
{
	zcbor_print("%s\r\n", __func__);

	bool tmp_result = (((zcbor_uint32_decode(state, (&(*result))))
	&& ((((*result) <= UINT32_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))));

	if (!tmp_result)
		zcbor_trace();

	return tmp_result;
}

static bool decode_repeated_bm_Ack_dst_ipv6_addr_uint(
		zcbor_state_t *state, uint32_t *result)
{
	zcbor_print("%s\r\n", __func__);

	bool tmp_result = (((zcbor_uint32_decode(state, (&(*result))))
	&& ((((*result) <= UINT32_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))));

	if (!tmp_result)
		zcbor_trace();

	return tmp_result;
}

static bool decode_repeated_bm_Heartbeat_src_ipv6_addr_uint(
		zcbor_state_t *state, uint32_t *result)
{
	zcbor_print("%s\r\n", __func__);

	bool tmp_result = (((zcbor_uint32_decode(state, (&(*result))))
	&& ((((*result) <= UINT32_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))));

	if (!tmp_result)
		zcbor_trace();

	return tmp_result;
}

static bool decode_repeated_bm_Request_Table_src_ipv6_addr_uint(
		zcbor_state_t *state, uint32_t *result)
{
	zcbor_print("%s\r\n", __func__);

	bool tmp_result = (((zcbor_uint32_decode(state, (&(*result))))
	&& ((((*result) <= UINT32_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))));

	if (!tmp_result)
		zcbor_trace();

	return tmp_result;
}

static bool decode_repeated_bm_Table_Response_src_ipv6_addr_uint(
		zcbor_state_t *state, uint32_t *result)
{
	zcbor_print("%s\r\n", __func__);

	bool tmp_result = (((zcbor_uint32_decode(state, (&(*result))))
	&& ((((*result) <= UINT32_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))));

	if (!tmp_result)
		zcbor_trace();

	return tmp_result;
}

static bool decode_repeated_bm_Table_Response_dst_ipv6_addr_uint(
		zcbor_state_t *state, uint32_t *result)
{
	zcbor_print("%s\r\n", __func__);

	bool tmp_result = (((zcbor_uint32_decode(state, (&(*result))))
	&& ((((*result) <= UINT32_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))));

	if (!tmp_result)
		zcbor_trace();

	return tmp_result;
}

static bool decode_bm_Temperature(
		zcbor_state_t *state, struct bm_Temperature *result)
{
	zcbor_print("%s\r\n", __func__);

	bool tmp_result = (((zcbor_list_start_decode(state) && ((((zcbor_uint32_decode(state, (&(*result)._bm_Temperature_temp)))
	&& ((((*result)._bm_Temperature_temp <= UINT32_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false)))
	&& ((decode_bm_Time(state, (&(*result)._bm_Temperature_timestamp))))) || (zcbor_list_map_end_force_decode(state), false)) && zcbor_list_end_decode(state))));

	if (!tmp_result)
		zcbor_trace();

	return tmp_result;
}

static bool decode_bm_Request_Table(
		zcbor_state_t *state, struct bm_Request_Table *result)
{
	zcbor_print("%s\r\n", __func__);

	bool tmp_result = (((zcbor_list_start_decode(state) && ((((zcbor_list_start_decode(state) && ((zcbor_multi_decode(0, 4, &(*result)._bm_Request_Table_src_ipv6_addr_uint_count, (zcbor_decoder_t *)decode_repeated_bm_Request_Table_src_ipv6_addr_uint, state, (&(*result)._bm_Request_Table_src_ipv6_addr_uint), sizeof(uint32_t))) || (zcbor_list_map_end_force_decode(state), false)) && zcbor_list_end_decode(state)))) || (zcbor_list_map_end_force_decode(state), false)) && zcbor_list_end_decode(state))));

	if (!tmp_result)
		zcbor_trace();

	return tmp_result;
}

static bool decode_bm_Heartbeat(
		zcbor_state_t *state, struct bm_Heartbeat *result)
{
	zcbor_print("%s\r\n", __func__);

	bool tmp_result = (((zcbor_list_start_decode(state) && ((((zcbor_list_start_decode(state) && ((zcbor_multi_decode(0, 4, &(*result)._bm_Heartbeat_src_ipv6_addr_uint_count, (zcbor_decoder_t *)decode_repeated_bm_Heartbeat_src_ipv6_addr_uint, state, (&(*result)._bm_Heartbeat_src_ipv6_addr_uint), sizeof(uint32_t))) || (zcbor_list_map_end_force_decode(state), false)) && zcbor_list_end_decode(state)))) || (zcbor_list_map_end_force_decode(state), false)) && zcbor_list_end_decode(state))));

	if (!tmp_result)
		zcbor_trace();

	return tmp_result;
}

static bool decode_bm_Header(
		zcbor_state_t *state, struct bm_Header *result)
{
	zcbor_print("%s\r\n", __func__);

	bool tmp_result = (((zcbor_list_start_decode(state) && ((((zcbor_uint32_decode(state, (&(*result)._bm_Header_seq)))
	&& ((((*result)._bm_Header_seq <= UINT32_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false)))
	&& ((decode_bm_Time(state, (&(*result)._bm_Header_stamp))))
	&& ((zcbor_tstr_decode(state, (&(*result)._bm_Header_frame_id)))
	&& ((((*result)._bm_Header_frame_id.len >= 2)
	&& ((*result)._bm_Header_frame_id.len <= 64)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false)))) || (zcbor_list_map_end_force_decode(state), false)) && zcbor_list_end_decode(state))));

	if (!tmp_result)
		zcbor_trace();

	return tmp_result;
}

static bool decode_bm_Ack(
		zcbor_state_t *state, struct bm_Ack *result)
{
	zcbor_print("%s\r\n", __func__);

	bool tmp_result = (((zcbor_list_start_decode(state) && ((((zcbor_list_start_decode(state) && ((zcbor_multi_decode(0, 4, &(*result)._bm_Ack_src_ipv6_addr_uint_count, (zcbor_decoder_t *)decode_repeated_bm_Ack_src_ipv6_addr_uint, state, (&(*result)._bm_Ack_src_ipv6_addr_uint), sizeof(uint32_t))) || (zcbor_list_map_end_force_decode(state), false)) && zcbor_list_end_decode(state)))
	&& ((zcbor_list_start_decode(state) && ((zcbor_multi_decode(0, 4, &(*result)._bm_Ack_dst_ipv6_addr_uint_count, (zcbor_decoder_t *)decode_repeated_bm_Ack_dst_ipv6_addr_uint, state, (&(*result)._bm_Ack_dst_ipv6_addr_uint), sizeof(uint32_t))) || (zcbor_list_map_end_force_decode(state), false)) && zcbor_list_end_decode(state)))) || (zcbor_list_map_end_force_decode(state), false)) && zcbor_list_end_decode(state))));

	if (!tmp_result)
		zcbor_trace();

	return tmp_result;
}

static bool decode_bm_Duration(
		zcbor_state_t *state, struct bm_Duration *result)
{
	zcbor_print("%s\r\n", __func__);

	bool tmp_result = (((zcbor_list_start_decode(state) && ((((zcbor_int32_decode(state, (&(*result)._bm_Duration_sec)))
	&& ((((*result)._bm_Duration_sec >= -2147483647)
	&& ((*result)._bm_Duration_sec <= INT32_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false)))
	&& ((zcbor_uint32_decode(state, (&(*result)._bm_Duration_nanosec)))
	&& ((((*result)._bm_Duration_nanosec <= UINT32_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false)))) || (zcbor_list_map_end_force_decode(state), false)) && zcbor_list_end_decode(state))));

	if (!tmp_result)
		zcbor_trace();

	return tmp_result;
}

static bool decode_bm_Table_Response(
		zcbor_state_t *state, struct bm_Table_Response *result)
{
	zcbor_print("%s\r\n", __func__);

	bool tmp_result = (((zcbor_list_start_decode(state) && ((((zcbor_list_start_decode(state) && ((zcbor_multi_decode(0, 3, &(*result)._bm_Table_Response_neighbors__bm_Neighbor_Info_count, (zcbor_decoder_t *)decode_bm_Neighbor_Info, state, (&(*result)._bm_Table_Response_neighbors__bm_Neighbor_Info), sizeof(struct bm_Neighbor_Info))) || (zcbor_list_map_end_force_decode(state), false)) && zcbor_list_end_decode(state)))
	&& ((zcbor_list_start_decode(state) && ((zcbor_multi_decode(0, 4, &(*result)._bm_Table_Response_src_ipv6_addr_uint_count, (zcbor_decoder_t *)decode_repeated_bm_Table_Response_src_ipv6_addr_uint, state, (&(*result)._bm_Table_Response_src_ipv6_addr_uint), sizeof(uint32_t))) || (zcbor_list_map_end_force_decode(state), false)) && zcbor_list_end_decode(state)))
	&& ((zcbor_list_start_decode(state) && ((zcbor_multi_decode(0, 4, &(*result)._bm_Table_Response_dst_ipv6_addr_uint_count, (zcbor_decoder_t *)decode_repeated_bm_Table_Response_dst_ipv6_addr_uint, state, (&(*result)._bm_Table_Response_dst_ipv6_addr_uint), sizeof(uint32_t))) || (zcbor_list_map_end_force_decode(state), false)) && zcbor_list_end_decode(state)))) || (zcbor_list_map_end_force_decode(state), false)) && zcbor_list_end_decode(state))));

	if (!tmp_result)
		zcbor_trace();

	return tmp_result;
}

static bool decode_bm_Time(
		zcbor_state_t *state, struct bm_Time *result)
{
	zcbor_print("%s\r\n", __func__);

	bool tmp_result = (((zcbor_list_start_decode(state) && ((((zcbor_int32_decode(state, (&(*result)._bm_Time_sec)))
	&& ((((*result)._bm_Time_sec >= -2147483647)
	&& ((*result)._bm_Time_sec <= INT32_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false)))
	&& ((zcbor_uint32_decode(state, (&(*result)._bm_Time_nanosec)))
	&& ((((*result)._bm_Time_nanosec <= UINT32_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false)))) || (zcbor_list_map_end_force_decode(state), false)) && zcbor_list_end_decode(state))));

	if (!tmp_result)
		zcbor_trace();

	return tmp_result;
}

static bool decode_bm_Neighbor_Info(
		zcbor_state_t *state, struct bm_Neighbor_Info *result)
{
	zcbor_print("%s\r\n", __func__);

	bool tmp_result = (((zcbor_list_start_decode(state) && ((((zcbor_list_start_decode(state) && ((zcbor_multi_decode(0, 4, &(*result)._bm_Neighbor_Info_ipv6_addr_uint_count, (zcbor_decoder_t *)decode_repeated_bm_Neighbor_Info_ipv6_addr_uint, state, (&(*result)._bm_Neighbor_Info_ipv6_addr_uint), sizeof(uint32_t))) || (zcbor_list_map_end_force_decode(state), false)) && zcbor_list_end_decode(state)))
	&& ((zcbor_uint32_decode(state, (&(*result)._bm_Neighbor_Info_reachable)))
	&& ((((*result)._bm_Neighbor_Info_reachable <= UINT8_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false)))
	&& ((zcbor_uint32_decode(state, (&(*result)._bm_Neighbor_Info_port)))
	&& ((((*result)._bm_Neighbor_Info_port <= UINT8_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false)))) || (zcbor_list_map_end_force_decode(state), false)) && zcbor_list_end_decode(state))));

	if (!tmp_result)
		zcbor_trace();

	return tmp_result;
}

static bool decode_bm_Discover_Neighbors(
		zcbor_state_t *state, struct bm_Discover_Neighbors *result)
{
	zcbor_print("%s\r\n", __func__);

	bool tmp_result = (((zcbor_list_start_decode(state) && ((((zcbor_list_start_decode(state) && ((zcbor_multi_decode(0, 4, &(*result)._bm_Discover_Neighbors_src_ipv6_addr_uint_count, (zcbor_decoder_t *)decode_repeated_bm_Discover_Neighbors_src_ipv6_addr_uint, state, (&(*result)._bm_Discover_Neighbors_src_ipv6_addr_uint), sizeof(uint32_t))) || (zcbor_list_map_end_force_decode(state), false)) && zcbor_list_end_decode(state)))) || (zcbor_list_map_end_force_decode(state), false)) && zcbor_list_end_decode(state))));

	if (!tmp_result)
		zcbor_trace();

	return tmp_result;
}



int cbor_decode_bm_Discover_Neighbors(
		const uint8_t *payload, size_t payload_len,
		struct bm_Discover_Neighbors *result,
		size_t *payload_len_out)
{
	zcbor_state_t states[4];

	zcbor_new_state(states, sizeof(states) / sizeof(zcbor_state_t), payload, payload_len, 1);

	bool ret = decode_bm_Discover_Neighbors(states, result);

	if (ret && (payload_len_out != NULL)) {
		*payload_len_out = MIN(payload_len,
				(size_t)states[0].payload - (size_t)payload);
	}

	if (!ret) {
		int err = zcbor_pop_error(states);

		zcbor_print("Return error: %d\r\n", err);
		return (err == ZCBOR_SUCCESS) ? ZCBOR_ERR_UNKNOWN : err;
	}
	return ZCBOR_SUCCESS;
}


int cbor_decode_bm_Neighbor_Info(
		const uint8_t *payload, size_t payload_len,
		struct bm_Neighbor_Info *result,
		size_t *payload_len_out)
{
	zcbor_state_t states[4];

	zcbor_new_state(states, sizeof(states) / sizeof(zcbor_state_t), payload, payload_len, 1);

	bool ret = decode_bm_Neighbor_Info(states, result);

	if (ret && (payload_len_out != NULL)) {
		*payload_len_out = MIN(payload_len,
				(size_t)states[0].payload - (size_t)payload);
	}

	if (!ret) {
		int err = zcbor_pop_error(states);

		zcbor_print("Return error: %d\r\n", err);
		return (err == ZCBOR_SUCCESS) ? ZCBOR_ERR_UNKNOWN : err;
	}
	return ZCBOR_SUCCESS;
}


int cbor_decode_bm_Time(
		const uint8_t *payload, size_t payload_len,
		struct bm_Time *result,
		size_t *payload_len_out)
{
	zcbor_state_t states[3];

	zcbor_new_state(states, sizeof(states) / sizeof(zcbor_state_t), payload, payload_len, 1);

	bool ret = decode_bm_Time(states, result);

	if (ret && (payload_len_out != NULL)) {
		*payload_len_out = MIN(payload_len,
				(size_t)states[0].payload - (size_t)payload);
	}

	if (!ret) {
		int err = zcbor_pop_error(states);

		zcbor_print("Return error: %d\r\n", err);
		return (err == ZCBOR_SUCCESS) ? ZCBOR_ERR_UNKNOWN : err;
	}
	return ZCBOR_SUCCESS;
}


int cbor_decode_bm_Table_Response(
		const uint8_t *payload, size_t payload_len,
		struct bm_Table_Response *result,
		size_t *payload_len_out)
{
	zcbor_state_t states[6];

	zcbor_new_state(states, sizeof(states) / sizeof(zcbor_state_t), payload, payload_len, 1);

	bool ret = decode_bm_Table_Response(states, result);

	if (ret && (payload_len_out != NULL)) {
		*payload_len_out = MIN(payload_len,
				(size_t)states[0].payload - (size_t)payload);
	}

	if (!ret) {
		int err = zcbor_pop_error(states);

		zcbor_print("Return error: %d\r\n", err);
		return (err == ZCBOR_SUCCESS) ? ZCBOR_ERR_UNKNOWN : err;
	}
	return ZCBOR_SUCCESS;
}


int cbor_decode_bm_Duration(
		const uint8_t *payload, size_t payload_len,
		struct bm_Duration *result,
		size_t *payload_len_out)
{
	zcbor_state_t states[3];

	zcbor_new_state(states, sizeof(states) / sizeof(zcbor_state_t), payload, payload_len, 1);

	bool ret = decode_bm_Duration(states, result);

	if (ret && (payload_len_out != NULL)) {
		*payload_len_out = MIN(payload_len,
				(size_t)states[0].payload - (size_t)payload);
	}

	if (!ret) {
		int err = zcbor_pop_error(states);

		zcbor_print("Return error: %d\r\n", err);
		return (err == ZCBOR_SUCCESS) ? ZCBOR_ERR_UNKNOWN : err;
	}
	return ZCBOR_SUCCESS;
}


int cbor_decode_bm_Ack(
		const uint8_t *payload, size_t payload_len,
		struct bm_Ack *result,
		size_t *payload_len_out)
{
	zcbor_state_t states[4];

	zcbor_new_state(states, sizeof(states) / sizeof(zcbor_state_t), payload, payload_len, 1);

	bool ret = decode_bm_Ack(states, result);

	if (ret && (payload_len_out != NULL)) {
		*payload_len_out = MIN(payload_len,
				(size_t)states[0].payload - (size_t)payload);
	}

	if (!ret) {
		int err = zcbor_pop_error(states);

		zcbor_print("Return error: %d\r\n", err);
		return (err == ZCBOR_SUCCESS) ? ZCBOR_ERR_UNKNOWN : err;
	}
	return ZCBOR_SUCCESS;
}


int cbor_decode_bm_Header(
		const uint8_t *payload, size_t payload_len,
		struct bm_Header *result,
		size_t *payload_len_out)
{
	zcbor_state_t states[4];

	zcbor_new_state(states, sizeof(states) / sizeof(zcbor_state_t), payload, payload_len, 1);

	bool ret = decode_bm_Header(states, result);

	if (ret && (payload_len_out != NULL)) {
		*payload_len_out = MIN(payload_len,
				(size_t)states[0].payload - (size_t)payload);
	}

	if (!ret) {
		int err = zcbor_pop_error(states);

		zcbor_print("Return error: %d\r\n", err);
		return (err == ZCBOR_SUCCESS) ? ZCBOR_ERR_UNKNOWN : err;
	}
	return ZCBOR_SUCCESS;
}


int cbor_decode_bm_Heartbeat(
		const uint8_t *payload, size_t payload_len,
		struct bm_Heartbeat *result,
		size_t *payload_len_out)
{
	zcbor_state_t states[4];

	zcbor_new_state(states, sizeof(states) / sizeof(zcbor_state_t), payload, payload_len, 1);

	bool ret = decode_bm_Heartbeat(states, result);

	if (ret && (payload_len_out != NULL)) {
		*payload_len_out = MIN(payload_len,
				(size_t)states[0].payload - (size_t)payload);
	}

	if (!ret) {
		int err = zcbor_pop_error(states);

		zcbor_print("Return error: %d\r\n", err);
		return (err == ZCBOR_SUCCESS) ? ZCBOR_ERR_UNKNOWN : err;
	}
	return ZCBOR_SUCCESS;
}


int cbor_decode_bm_Request_Table(
		const uint8_t *payload, size_t payload_len,
		struct bm_Request_Table *result,
		size_t *payload_len_out)
{
	zcbor_state_t states[4];

	zcbor_new_state(states, sizeof(states) / sizeof(zcbor_state_t), payload, payload_len, 1);

	bool ret = decode_bm_Request_Table(states, result);

	if (ret && (payload_len_out != NULL)) {
		*payload_len_out = MIN(payload_len,
				(size_t)states[0].payload - (size_t)payload);
	}

	if (!ret) {
		int err = zcbor_pop_error(states);

		zcbor_print("Return error: %d\r\n", err);
		return (err == ZCBOR_SUCCESS) ? ZCBOR_ERR_UNKNOWN : err;
	}
	return ZCBOR_SUCCESS;
}


int cbor_decode_bm_Temperature(
		const uint8_t *payload, size_t payload_len,
		struct bm_Temperature *result,
		size_t *payload_len_out)
{
	zcbor_state_t states[4];

	zcbor_new_state(states, sizeof(states) / sizeof(zcbor_state_t), payload, payload_len, 1);

	bool ret = decode_bm_Temperature(states, result);

	if (ret && (payload_len_out != NULL)) {
		*payload_len_out = MIN(payload_len,
				(size_t)states[0].payload - (size_t)payload);
	}

	if (!ret) {
		int err = zcbor_pop_error(states);

		zcbor_print("Return error: %d\r\n", err);
		return (err == ZCBOR_SUCCESS) ? ZCBOR_ERR_UNKNOWN : err;
	}
	return ZCBOR_SUCCESS;
}
