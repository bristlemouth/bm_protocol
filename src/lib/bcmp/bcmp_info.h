#pragma once

#include <stdint.h>
#include "lwip/ip_addr.h"

#ifdef __cplusplus
extern "C" {
#endif

void bcmp_expect_info_from_node_id(uint64_t node_id);
err_t bcmp_request_info(uint64_t target_node_id, const ip_addr_t *addr);
err_t bcmp_process_info_request(bcmp_device_info_request_t *info_req, const ip_addr_t *src, const ip_addr_t *dst);
err_t bcmp_process_info_reply(bcmp_device_info_reply_t *dev_info);
#ifdef __cplusplus
}
#endif
