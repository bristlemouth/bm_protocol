#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

/*!
 * @brief Callback function for bm_service_request
 * @note Should not call bm_service_request from within this callback.
 * @param[in] ack True if the request was acknowledged, false if it was rejected.
 * @param[in] msg_id The message id of the request.
 * @param[in] service_strlen The length of the service string.
 * @param[in] service The service string.
 * @param[in] reply_len The length of the reply data.
 * @param[in] reply_data The reply data.
 * @return True if the reply was handled, false otherwise.
 */
typedef bool (*BmServiceReplyCb)(bool ack, uint32_t msg_id, size_t service_strlen,
                                 const char *service, size_t reply_len, uint8_t *reply_data);

void bm_service_request_init(void);
bool bm_service_request(size_t service_strlen, const char *service, size_t data_len,
                        const uint8_t *data, BmServiceReplyCb reply_cb, uint32_t timeout_s);
