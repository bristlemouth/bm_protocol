#pragma once

#include <stdint.h>
#include "lwip/ip_addr.h"

err_t bcmp_send_ping_request(uint64_t node_id, const ip_addr_t *addr, const uint8_t* payload, uint16_t payload_len);
err_t bcmp_send_ping_reply(bcmp_echo_reply_t *echo_reply, const ip_addr_t *addr, uint16_t seq_num);
err_t bcmp_process_ping_request(bcmp_echo_request_t *echo_req, const ip_addr_t *src, const ip_addr_t *dst, uint16_t seq_num);
err_t bcmp_process_ping_reply(bcmp_echo_reply_t *echo_reply);
