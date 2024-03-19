#pragma once

#include "bm_service_request.h"

typedef bool (*device_test_f)(void *in_data, uint32_t in_len, uint8_t **out_data,
                              uint32_t &out_len);

void device_test_service_init(device_test_f f);
bool device_test_service_request(uint64_t target_node_id, void *data, size_t len,
                                 bm_service_reply_cb reply_cb, uint32_t timeout_s);
