#pragma once

#include <stdint.h>

BmErr bcmp_send_ping_request(uint64_t node_id, const void *addr, const uint8_t* payload, uint16_t payload_len);
// BmErr bcmp_send_ping_reply(BcmpEchoReply *echo_reply, const void *addr, uint16_t seq_num);
// BmErr bcmp_process_ping_request(BcmpProcessData data, const void *src, const void *dst, uint16_t seq_num);
// BmErr bcmp_process_ping_reply(BcmpProcessData data);
BmErr ping_init(void);