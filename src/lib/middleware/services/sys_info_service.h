#pragma once

#include "configuration.h"
#include "bm_service_request.h"

void sys_info_service_init(cfg::Configuration& sys_config);
bool sys_info_service_request(uint64_t target_node_id, bm_service_reply_cb reply_cb, uint32_t timeout_s);
