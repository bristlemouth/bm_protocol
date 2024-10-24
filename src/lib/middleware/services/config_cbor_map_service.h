#pragma once

#include "configuration.h"

extern "C" {
#include "bm_service_request.h"
}

constexpr uint32_t CONFIG_CBOR_MAP_PARTITION_ID_SYS = 1;
constexpr uint32_t CONFIG_CBOR_MAP_PARTITION_ID_HW = 2;
constexpr uint32_t CONFIG_CBOR_MAP_PARTITION_ID_USER = 3;

void config_cbor_map_service_init(cfg::Configuration& hw_cfg, cfg::Configuration& sys_config, cfg::Configuration& user_config);
bool config_cbor_map_service_request(uint64_t target_node_id, uint32_t partition_id, BmServiceReplyCb reply_cb, uint32_t timeout_s);
