/*
 * Generated using zcbor version 0.5.99
 * https://github.com/NordicSemiconductor/zcbor
 * Generated with a --default-max-qty of 3
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include "zcbor_encode.h"
#include "bm_zcbor_encode.h"

#if DEFAULT_MAX_QTY != 3
#error "The type file was generated with a different default_max_qty than this file"
#endif

static bool encode_repeated_bm_Discover_Neighbors_src_ipv6_addr_uint(zcbor_state_t *state, const uint32_t *input);
static bool encode_repeated_bm_Neighbor_Info_ipv6_addr_uint(zcbor_state_t *state, const uint32_t *input);
static bool encode_repeated_bm_Ack_src_ipv6_addr_uint(zcbor_state_t *state, const uint32_t *input);
static bool encode_repeated_bm_Ack_dst_ipv6_addr_uint(zcbor_state_t *state, const uint32_t *input);
static bool encode_repeated_bm_Heartbeat_src_ipv6_addr_uint(zcbor_state_t *state, const uint32_t *input);
static bool encode_repeated_bm_Request_Table_src_ipv6_addr_uint(zcbor_state_t *state, const uint32_t *input);
static bool encode_repeated_bm_Table_Response_src_ipv6_addr_uint(zcbor_state_t *state, const uint32_t *input);
static bool encode_repeated_bm_Table_Response_dst_ipv6_addr_uint(zcbor_state_t *state, const uint32_t *input);
static bool encode_bm_Temperature(zcbor_state_t *state, const struct bm_Temperature *input);
static bool encode_bm_Request_Table(zcbor_state_t *state, const struct bm_Request_Table *input);
static bool encode_bm_Heartbeat(zcbor_state_t *state, const struct bm_Heartbeat *input);
static bool encode_bm_Header(zcbor_state_t *state, const struct bm_Header *input);
static bool encode_bm_Ack(zcbor_state_t *state, const struct bm_Ack *input);
static bool encode_bm_Duration(zcbor_state_t *state, const struct bm_Duration *input);
static bool encode_bm_Table_Response(zcbor_state_t *state, const struct bm_Table_Response *input);
static bool encode_bm_Time(zcbor_state_t *state, const struct bm_Time *input);
static bool encode_bm_Neighbor_Info(zcbor_state_t *state, const struct bm_Neighbor_Info *input);
static bool encode_bm_Discover_Neighbors(zcbor_state_t *state, const struct bm_Discover_Neighbors *input);


static bool encode_repeated_bm_Discover_Neighbors_src_ipv6_addr_uint(
		zcbor_state_t *state, const uint32_t *input)
{
	zcbor_print("%s\r\n", __func__);

	bool tmp_result = ((((((*input) <= 255)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint32_encode(state, (&(*input))))));

	if (!tmp_result)
		zcbor_trace();

	return tmp_result;
}

static bool encode_repeated_bm_Neighbor_Info_ipv6_addr_uint(
		zcbor_state_t *state, const uint32_t *input)
{
	zcbor_print("%s\r\n", __func__);

	bool tmp_result = ((((((*input) <= 255)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint32_encode(state, (&(*input))))));

	if (!tmp_result)
		zcbor_trace();

	return tmp_result;
}

static bool encode_repeated_bm_Ack_src_ipv6_addr_uint(
		zcbor_state_t *state, const uint32_t *input)
{
	zcbor_print("%s\r\n", __func__);

	bool tmp_result = ((((((*input) <= 255)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint32_encode(state, (&(*input))))));

	if (!tmp_result)
		zcbor_trace();

	return tmp_result;
}

static bool encode_repeated_bm_Ack_dst_ipv6_addr_uint(
		zcbor_state_t *state, const uint32_t *input)
{
	zcbor_print("%s\r\n", __func__);

	bool tmp_result = ((((((*input) <= 255)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint32_encode(state, (&(*input))))));

	if (!tmp_result)
		zcbor_trace();

	return tmp_result;
}

static bool encode_repeated_bm_Heartbeat_src_ipv6_addr_uint(
		zcbor_state_t *state, const uint32_t *input)
{
	zcbor_print("%s\r\n", __func__);

	bool tmp_result = ((((((*input) <= 255)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint32_encode(state, (&(*input))))));

	if (!tmp_result)
		zcbor_trace();

	return tmp_result;
}

static bool encode_repeated_bm_Request_Table_src_ipv6_addr_uint(
		zcbor_state_t *state, const uint32_t *input)
{
	zcbor_print("%s\r\n", __func__);

	bool tmp_result = ((((((*input) <= 255)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint32_encode(state, (&(*input))))));

	if (!tmp_result)
		zcbor_trace();

	return tmp_result;
}

static bool encode_repeated_bm_Table_Response_src_ipv6_addr_uint(
		zcbor_state_t *state, const uint32_t *input)
{
	zcbor_print("%s\r\n", __func__);

	bool tmp_result = ((((((*input) <= 255)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint32_encode(state, (&(*input))))));

	if (!tmp_result)
		zcbor_trace();

	return tmp_result;
}

static bool encode_repeated_bm_Table_Response_dst_ipv6_addr_uint(
		zcbor_state_t *state, const uint32_t *input)
{
	zcbor_print("%s\r\n", __func__);

	bool tmp_result = ((((((*input) <= 255)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint32_encode(state, (&(*input))))));

	if (!tmp_result)
		zcbor_trace();

	return tmp_result;
}

static bool encode_bm_Temperature(
		zcbor_state_t *state, const struct bm_Temperature *input)
{
	zcbor_print("%s\r\n", __func__);

	bool tmp_result = (((zcbor_list_start_encode(state, 2) && (((((((*input)._bm_Temperature_temp <= 4294967295)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint32_encode(state, (&(*input)._bm_Temperature_temp))))
	&& ((encode_bm_Time(state, (&(*input)._bm_Temperature_timestamp))))) || (zcbor_list_map_end_force_encode(state), false)) && zcbor_list_end_encode(state, 2))));

	if (!tmp_result)
		zcbor_trace();

	return tmp_result;
}

static bool encode_bm_Request_Table(
		zcbor_state_t *state, const struct bm_Request_Table *input)
{
	zcbor_print("%s\r\n", __func__);

	bool tmp_result = (((zcbor_list_start_encode(state, 1) && ((((zcbor_list_start_encode(state, 16) && ((zcbor_multi_encode_minmax(0, 16, &(*input)._bm_Request_Table_src_ipv6_addr_uint_count, (zcbor_encoder_t *)encode_repeated_bm_Request_Table_src_ipv6_addr_uint, state, (&(*input)._bm_Request_Table_src_ipv6_addr_uint), sizeof(uint32_t))) || (zcbor_list_map_end_force_encode(state), false)) && zcbor_list_end_encode(state, 16)))) || (zcbor_list_map_end_force_encode(state), false)) && zcbor_list_end_encode(state, 1))));

	if (!tmp_result)
		zcbor_trace();

	return tmp_result;
}

static bool encode_bm_Heartbeat(
		zcbor_state_t *state, const struct bm_Heartbeat *input)
{
	zcbor_print("%s\r\n", __func__);

	bool tmp_result = (((zcbor_list_start_encode(state, 1) && ((((zcbor_list_start_encode(state, 16) && ((zcbor_multi_encode_minmax(0, 16, &(*input)._bm_Heartbeat_src_ipv6_addr_uint_count, (zcbor_encoder_t *)encode_repeated_bm_Heartbeat_src_ipv6_addr_uint, state, (&(*input)._bm_Heartbeat_src_ipv6_addr_uint), sizeof(uint32_t))) || (zcbor_list_map_end_force_encode(state), false)) && zcbor_list_end_encode(state, 16)))) || (zcbor_list_map_end_force_encode(state), false)) && zcbor_list_end_encode(state, 1))));

	if (!tmp_result)
		zcbor_trace();

	return tmp_result;
}

static bool encode_bm_Header(
		zcbor_state_t *state, const struct bm_Header *input)
{
	zcbor_print("%s\r\n", __func__);

	bool tmp_result = (((zcbor_list_start_encode(state, 3) && (((((((*input)._bm_Header_seq <= 4294967295)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint32_encode(state, (&(*input)._bm_Header_seq))))
	&& ((encode_bm_Time(state, (&(*input)._bm_Header_stamp))))
	&& (((((*input)._bm_Header_frame_id.len >= 2)
	&& ((*input)._bm_Header_frame_id.len <= 64)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_tstr_encode(state, (&(*input)._bm_Header_frame_id))))) || (zcbor_list_map_end_force_encode(state), false)) && zcbor_list_end_encode(state, 3))));

	if (!tmp_result)
		zcbor_trace();

	return tmp_result;
}

static bool encode_bm_Ack(
		zcbor_state_t *state, const struct bm_Ack *input)
{
	zcbor_print("%s\r\n", __func__);

	bool tmp_result = (((zcbor_list_start_encode(state, 2) && ((((zcbor_list_start_encode(state, 16) && ((zcbor_multi_encode_minmax(0, 16, &(*input)._bm_Ack_src_ipv6_addr_uint_count, (zcbor_encoder_t *)encode_repeated_bm_Ack_src_ipv6_addr_uint, state, (&(*input)._bm_Ack_src_ipv6_addr_uint), sizeof(uint32_t))) || (zcbor_list_map_end_force_encode(state), false)) && zcbor_list_end_encode(state, 16)))
	&& ((zcbor_list_start_encode(state, 16) && ((zcbor_multi_encode_minmax(0, 16, &(*input)._bm_Ack_dst_ipv6_addr_uint_count, (zcbor_encoder_t *)encode_repeated_bm_Ack_dst_ipv6_addr_uint, state, (&(*input)._bm_Ack_dst_ipv6_addr_uint), sizeof(uint32_t))) || (zcbor_list_map_end_force_encode(state), false)) && zcbor_list_end_encode(state, 16)))) || (zcbor_list_map_end_force_encode(state), false)) && zcbor_list_end_encode(state, 2))));

	if (!tmp_result)
		zcbor_trace();

	return tmp_result;
}

static bool encode_bm_Duration(
		zcbor_state_t *state, const struct bm_Duration *input)
{
	zcbor_print("%s\r\n", __func__);

	bool tmp_result = (((zcbor_list_start_encode(state, 2) && (((((((*input)._bm_Duration_sec >= -2147483647)
	&& ((*input)._bm_Duration_sec <= 2147483647)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_int32_encode(state, (&(*input)._bm_Duration_sec))))
	&& (((((*input)._bm_Duration_nanosec <= 4294967295)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint32_encode(state, (&(*input)._bm_Duration_nanosec))))) || (zcbor_list_map_end_force_encode(state), false)) && zcbor_list_end_encode(state, 2))));

	if (!tmp_result)
		zcbor_trace();

	return tmp_result;
}

static bool encode_bm_Table_Response(
		zcbor_state_t *state, const struct bm_Table_Response *input)
{
	zcbor_print("%s\r\n", __func__);

	bool tmp_result = (((zcbor_list_start_encode(state, 3) && ((((zcbor_list_start_encode(state, 3) && ((zcbor_multi_encode_minmax(0, 3, &(*input)._bm_Table_Response_neighbors__bm_Neighbor_Info_count, (zcbor_encoder_t *)encode_bm_Neighbor_Info, state, (&(*input)._bm_Table_Response_neighbors__bm_Neighbor_Info), sizeof(struct bm_Neighbor_Info))) || (zcbor_list_map_end_force_encode(state), false)) && zcbor_list_end_encode(state, 3)))
	&& ((zcbor_list_start_encode(state, 16) && ((zcbor_multi_encode_minmax(0, 16, &(*input)._bm_Table_Response_src_ipv6_addr_uint_count, (zcbor_encoder_t *)encode_repeated_bm_Table_Response_src_ipv6_addr_uint, state, (&(*input)._bm_Table_Response_src_ipv6_addr_uint), sizeof(uint32_t))) || (zcbor_list_map_end_force_encode(state), false)) && zcbor_list_end_encode(state, 16)))
	&& ((zcbor_list_start_encode(state, 16) && ((zcbor_multi_encode_minmax(0, 16, &(*input)._bm_Table_Response_dst_ipv6_addr_uint_count, (zcbor_encoder_t *)encode_repeated_bm_Table_Response_dst_ipv6_addr_uint, state, (&(*input)._bm_Table_Response_dst_ipv6_addr_uint), sizeof(uint32_t))) || (zcbor_list_map_end_force_encode(state), false)) && zcbor_list_end_encode(state, 16)))) || (zcbor_list_map_end_force_encode(state), false)) && zcbor_list_end_encode(state, 3))));

	if (!tmp_result)
		zcbor_trace();

	return tmp_result;
}

static bool encode_bm_Time(
		zcbor_state_t *state, const struct bm_Time *input)
{
	zcbor_print("%s\r\n", __func__);

	bool tmp_result = (((zcbor_list_start_encode(state, 2) && (((((((*input)._bm_Time_sec >= -2147483647)
	&& ((*input)._bm_Time_sec <= 2147483647)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_int32_encode(state, (&(*input)._bm_Time_sec))))
	&& (((((*input)._bm_Time_nanosec <= 4294967295)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint32_encode(state, (&(*input)._bm_Time_nanosec))))) || (zcbor_list_map_end_force_encode(state), false)) && zcbor_list_end_encode(state, 2))));

	if (!tmp_result)
		zcbor_trace();

	return tmp_result;
}

static bool encode_bm_Neighbor_Info(
		zcbor_state_t *state, const struct bm_Neighbor_Info *input)
{
	zcbor_print("%s\r\n", __func__);

	bool tmp_result = (((zcbor_list_start_encode(state, 3) && ((((zcbor_list_start_encode(state, 16) && ((zcbor_multi_encode_minmax(0, 16, &(*input)._bm_Neighbor_Info_ipv6_addr_uint_count, (zcbor_encoder_t *)encode_repeated_bm_Neighbor_Info_ipv6_addr_uint, state, (&(*input)._bm_Neighbor_Info_ipv6_addr_uint), sizeof(uint32_t))) || (zcbor_list_map_end_force_encode(state), false)) && zcbor_list_end_encode(state, 16)))
	&& (((((*input)._bm_Neighbor_Info_reachable <= 255)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint32_encode(state, (&(*input)._bm_Neighbor_Info_reachable))))
	&& (((((*input)._bm_Neighbor_Info_port <= 255)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint32_encode(state, (&(*input)._bm_Neighbor_Info_port))))) || (zcbor_list_map_end_force_encode(state), false)) && zcbor_list_end_encode(state, 3))));

	if (!tmp_result)
		zcbor_trace();

	return tmp_result;
}

static bool encode_bm_Discover_Neighbors(
		zcbor_state_t *state, const struct bm_Discover_Neighbors *input)
{
	zcbor_print("%s\r\n", __func__);

	bool tmp_result = (((zcbor_list_start_encode(state, 1) && ((((zcbor_list_start_encode(state, 16) && ((zcbor_multi_encode_minmax(0, 16, &(*input)._bm_Discover_Neighbors_src_ipv6_addr_uint_count, (zcbor_encoder_t *)encode_repeated_bm_Discover_Neighbors_src_ipv6_addr_uint, state, (&(*input)._bm_Discover_Neighbors_src_ipv6_addr_uint), sizeof(uint32_t))) || (zcbor_list_map_end_force_encode(state), false)) && zcbor_list_end_encode(state, 16)))) || (zcbor_list_map_end_force_encode(state), false)) && zcbor_list_end_encode(state, 1))));

	if (!tmp_result)
		zcbor_trace();

	return tmp_result;
}



int cbor_encode_bm_Discover_Neighbors(
		uint8_t *payload, size_t payload_len,
		const struct bm_Discover_Neighbors *input,
		size_t *payload_len_out)
{
	zcbor_state_t states[4];

	zcbor_new_state(states, sizeof(states) / sizeof(zcbor_state_t), payload, payload_len, 1);

	bool ret = encode_bm_Discover_Neighbors(states, input);

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


int cbor_encode_bm_Neighbor_Info(
		uint8_t *payload, size_t payload_len,
		const struct bm_Neighbor_Info *input,
		size_t *payload_len_out)
{
	zcbor_state_t states[4];

	zcbor_new_state(states, sizeof(states) / sizeof(zcbor_state_t), payload, payload_len, 1);

	bool ret = encode_bm_Neighbor_Info(states, input);

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


int cbor_encode_bm_Time(
		uint8_t *payload, size_t payload_len,
		const struct bm_Time *input,
		size_t *payload_len_out)
{
	zcbor_state_t states[3];

	zcbor_new_state(states, sizeof(states) / sizeof(zcbor_state_t), payload, payload_len, 1);

	bool ret = encode_bm_Time(states, input);

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


int cbor_encode_bm_Table_Response(
		uint8_t *payload, size_t payload_len,
		const struct bm_Table_Response *input,
		size_t *payload_len_out)
{
	zcbor_state_t states[6];

	zcbor_new_state(states, sizeof(states) / sizeof(zcbor_state_t), payload, payload_len, 1);

	bool ret = encode_bm_Table_Response(states, input);

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


int cbor_encode_bm_Duration(
		uint8_t *payload, size_t payload_len,
		const struct bm_Duration *input,
		size_t *payload_len_out)
{
	zcbor_state_t states[3];

	zcbor_new_state(states, sizeof(states) / sizeof(zcbor_state_t), payload, payload_len, 1);

	bool ret = encode_bm_Duration(states, input);

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


int cbor_encode_bm_Ack(
		uint8_t *payload, size_t payload_len,
		const struct bm_Ack *input,
		size_t *payload_len_out)
{
	zcbor_state_t states[4];

	zcbor_new_state(states, sizeof(states) / sizeof(zcbor_state_t), payload, payload_len, 1);

	bool ret = encode_bm_Ack(states, input);

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


int cbor_encode_bm_Header(
		uint8_t *payload, size_t payload_len,
		const struct bm_Header *input,
		size_t *payload_len_out)
{
	zcbor_state_t states[4];

	zcbor_new_state(states, sizeof(states) / sizeof(zcbor_state_t), payload, payload_len, 1);

	bool ret = encode_bm_Header(states, input);

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


int cbor_encode_bm_Heartbeat(
		uint8_t *payload, size_t payload_len,
		const struct bm_Heartbeat *input,
		size_t *payload_len_out)
{
	zcbor_state_t states[4];

	zcbor_new_state(states, sizeof(states) / sizeof(zcbor_state_t), payload, payload_len, 1);

	bool ret = encode_bm_Heartbeat(states, input);

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


int cbor_encode_bm_Request_Table(
		uint8_t *payload, size_t payload_len,
		const struct bm_Request_Table *input,
		size_t *payload_len_out)
{
	zcbor_state_t states[4];

	zcbor_new_state(states, sizeof(states) / sizeof(zcbor_state_t), payload, payload_len, 1);

	bool ret = encode_bm_Request_Table(states, input);

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


int cbor_encode_bm_Temperature(
		uint8_t *payload, size_t payload_len,
		const struct bm_Temperature *input,
		size_t *payload_len_out)
{
	zcbor_state_t states[4];

	zcbor_new_state(states, sizeof(states) / sizeof(zcbor_state_t), payload, payload_len, 1);

	bool ret = encode_bm_Temperature(states, input);

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
