#pragma once

#include <stdint.h>
#include <stdlib.h>

#include "bm_service_request.h"
#include "bm_pubsub.h"

#define BM_SERVICE_MAX_SERVICE_STRLEN (BM_TOPIC_MAX_LEN - 4) // 4 is from strlen(BM_SERVICE_REQ_STR)

/*!
 * @brief Callback function for bm_service
 * @param[in] service_strlen The length of the service string.
 * @param[in] service The service string.
 * @param[in] req_data_len The length of the request data.
 * @param[in] req_data The request data.
 * @param[in,out] buffer_len Passed in, this param is the size of the data buffer. On return, this param is the length of the reply data.
 * @param[out] reply_data The reply data buffer to be filled in.
 * @return True if the request was handled, false otherwise.
 */
typedef bool (*BmServiceHandler)(size_t service_strlen,
                                    const char * service,
                                    size_t req_data_len,
                                    uint8_t * req_data,
                                    size_t &buffer_len,
                                    uint8_t * reply_data);

void bm_service_init(void);
bool bm_service_register(size_t service_strlen, const char * service, BmServiceHandler service_handler);
bool bm_service_unregister(size_t service_strlen, const char * service);