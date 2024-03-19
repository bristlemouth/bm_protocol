#pragma once

#include "bm_service_request.h"

/*!
 * @brief Function pointer type for device test service.
 *
 * @param success Set to true if the test was successful, false otherwise.
 * @param in_data Input data for the test.
 * @param in_len Length of the input data.
 * @param out_len Length of the output data.
 * @return Pointer to the allocatedoutput data.
 * (Can be NULL if out_len is 0, device_test_service will free the memory.)
*/
typedef uint8_t *(*device_test_f)(bool &success, void *in_data, uint32_t in_len,
                                  uint32_t &out_len);

void device_test_service_init(device_test_f f);
bool device_test_service_request(uint64_t target_node_id, void *data, size_t len,
                                 bm_service_reply_cb reply_cb, uint32_t timeout_s);
