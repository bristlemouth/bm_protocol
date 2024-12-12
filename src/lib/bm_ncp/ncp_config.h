#pragma once
#include "bm_common_structs.h"
#include "configuration.h"
#include <stdint.h>
#include <stdlib.h>

bool ncp_cfg_get_cb(uint64_t node_id, BmConfigPartition partition, size_t key_len,
                    const char *key);
bool ncp_cfg_set_cb(uint64_t node_id, BmConfigPartition partition, size_t key_len,
                    const char *key, size_t value_size, void *val);
bool ncp_cfg_commit_cb(uint64_t node_id, BmConfigPartition partition);
bool ncp_cfg_status_request_cb(uint64_t node_id, BmConfigPartition partition);
bool ncp_cfg_key_del_request_cb(uint64_t node_id, BmConfigPartition partition,
                                size_t key_len, const char *key);
void ncp_cfg_init(void);
