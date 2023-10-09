#pragma once

#include "configuration.h"
#include "bm_service_request.h"

typedef struct sys_info_service_data {
    uint64_t node_id;
    uint32_t gitSHA;
    uint32_t sys_config_crc;
    size_t app_name_strlen;
    char app_name[0];
} __attribute__ ((packed)) sys_info_service_data_s; // Note: To be moved. 

void sys_info_service_init(cfg::Configuration& sys_config);
bool sys_info_service_request(uint64_t target_node_id, bm_service_reply_cb reply_cb, uint32_t timeout_s);