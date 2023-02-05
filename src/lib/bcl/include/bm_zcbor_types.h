/*
 * Generated using zcbor version 0.5.99
 * https://github.com/NordicSemiconductor/zcbor
 * Generated with a --default-max-qty of 3
 */

#ifndef BM_ZCBOR_TYPES_H__
#define BM_ZCBOR_TYPES_H__

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include "zcbor_encode.h"

/** Which value for --default-max-qty this file was created with.
 *
 *  The define is used in the other generated file to do a build-time
 *  compatibility check.
 *
 *  See `zcbor --help` for more information about --default-max-qty
 */
#define DEFAULT_MAX_QTY 3

struct bm_Discover_Neighbors {
	uint32_t _bm_Discover_Neighbors_src_ipv6_addr_uint[16];
	uint_fast32_t _bm_Discover_Neighbors_src_ipv6_addr_uint_count;
};

struct bm_Neighbor_Info {
	uint32_t _bm_Neighbor_Info_ipv6_addr_uint[16];
	uint_fast32_t _bm_Neighbor_Info_ipv6_addr_uint_count;
	uint32_t _bm_Neighbor_Info_reachable;
	uint32_t _bm_Neighbor_Info_port;
};

struct bm_Time {
	int32_t _bm_Time_sec;
	uint32_t _bm_Time_nanosec;
};

struct bm_Duration {
	int32_t _bm_Duration_sec;
	uint32_t _bm_Duration_nanosec;
};

struct bm_Ack {
	uint32_t _bm_Ack_src_ipv6_addr_uint[16];
	uint_fast32_t _bm_Ack_src_ipv6_addr_uint_count;
	uint32_t _bm_Ack_dst_ipv6_addr_uint[16];
	uint_fast32_t _bm_Ack_dst_ipv6_addr_uint_count;
};

struct bm_Heartbeat {
	uint32_t _bm_Heartbeat_src_ipv6_addr_uint[16];
	uint_fast32_t _bm_Heartbeat_src_ipv6_addr_uint_count;
};

struct bm_Request_Table {
	uint32_t _bm_Request_Table_src_ipv6_addr_uint[16];
	uint_fast32_t _bm_Request_Table_src_ipv6_addr_uint_count;
};

struct bm_Table_Response {
	struct bm_Neighbor_Info _bm_Table_Response_neighbors__bm_Neighbor_Info[3];
	uint_fast32_t _bm_Table_Response_neighbors__bm_Neighbor_Info_count;
	uint32_t _bm_Table_Response_src_ipv6_addr_uint[16];
	uint_fast32_t _bm_Table_Response_src_ipv6_addr_uint_count;
	uint32_t _bm_Table_Response_dst_ipv6_addr_uint[16];
	uint_fast32_t _bm_Table_Response_dst_ipv6_addr_uint_count;
};

struct bm_Header {
	uint32_t _bm_Header_seq;
	struct bm_Time _bm_Header_stamp;
	struct zcbor_string _bm_Header_frame_id;
};

struct bm_Temperature {
	uint32_t _bm_Temperature_temp;
	struct bm_Time _bm_Temperature_timestamp;
};


#endif /* BM_ZCBOR_TYPES_H__ */
