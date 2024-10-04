#pragma once

#include "messages.h"
#include "bm_configs_generic.h"
#include "util.h"
#include <stdint.h>

BmErr bcmp_config_init(void);
bool bcmp_config_get(uint64_t target_node_id, BmConfigPartition partition,
                     size_t key_len, const char *key, BmErr &err,
                     BmErr (*reply_cb)(uint8_t *) = NULL);
bool bcmp_config_set(uint64_t target_node_id, BmConfigPartition partition,
                     size_t key_len, const char *key, size_t value_size, void *val, BmErr &err,
                     BmErr (*reply_cb)(uint8_t *) = NULL);
bool bcmp_config_commit(uint64_t target_node_id, BmConfigPartition partition,
                        BmErr &err);
bool bcmp_config_status_request(uint64_t target_node_id, BmConfigPartition partition,
                                BmErr &err, BmErr (*reply_cb)(uint8_t *) = NULL);
bool bcmp_config_status_response(uint64_t target_node_id,
                                 BmConfigPartition partition, bool commited,
                                 uint8_t num_keys, const GenericConfigKey *keys, BmErr &err,
                                 uint16_t seq_num);
bool bcmp_config_del_key(uint64_t target_node_id, BmConfigPartition partition,
                         size_t key_len, const char *key, BmErr (*reply_cb)(uint8_t *) = NULL);
