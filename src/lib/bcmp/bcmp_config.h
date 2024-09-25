#pragma once

#include "bcmp.h"
#include "bcmp_messages.h"
#include "bm_common_structs.h"
#include "configuration.h"
#include "util.h"
#include <stdint.h>

using namespace cfg;

BmErr bcmp_config_init(Configuration *user_cfg, Configuration *sys_cfg);
bool bcmp_config_get(uint64_t target_node_id, bm_common_config_partition_e partition,
                     size_t key_len, const char *key, err_t &err,
                     BmErr (*reply_cb)(uint8_t *) = NULL);
bool bcmp_config_set(uint64_t target_node_id, bm_common_config_partition_e partition,
                     size_t key_len, const char *key, size_t value_size, void *val, err_t &err,
                     BmErr (*reply_cb)(uint8_t *) = NULL);
bool bcmp_config_commit(uint64_t target_node_id, bm_common_config_partition_e partition,
                        err_t &err);
bool bcmp_config_status_request(uint64_t target_node_id, bm_common_config_partition_e partition,
                                err_t &err, BmErr (*reply_cb)(uint8_t *) = NULL);
bool bcmp_config_status_response(uint64_t target_node_id,
                                 bm_common_config_partition_e partition, bool commited,
                                 err_t &err);
bool bcmp_config_del_key(uint64_t target_node_id, bm_common_config_partition_e partition,
                         size_t key_len, const char *key, BmErr (*reply_cb)(uint8_t *) = NULL);
