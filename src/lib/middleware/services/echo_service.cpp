#include "echo_service.h"
#include "bm_service_common.h"
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
extern "C" {
#include "bm_service.h"
#include "device.h"
}

#define echo_service_suffix "/echo"

/*!
  @brief Handle Incoming Echo Service

  @details This takes the request message, and then copies it to reply
           to the message

  @param service_strlen length of service string
  @param service service string
  @param req_data_len length of request message
  @param req_data request data sent to service
  @param buffer_len length of reply buffer
  @param reply_data message to reply to request with

  @return
*/
static bool echo_service_handler(size_t service_strlen, const char *service,
                                 size_t req_data_len, uint8_t *req_data, size_t *buffer_len,
                                 uint8_t *reply_data) {
  bool rval = true;
  printf("Data received on service: %.*s\n", service_strlen, service);
  if (*buffer_len <= MAX_BM_SERVICE_DATA_SIZE) {
    *buffer_len = req_data_len;
    memcpy(reply_data, req_data, req_data_len);
  } else {
    *buffer_len = 0;
    rval = false;
  }
  return rval;
}

/*!
  @brief Initialize the echo service.
 */
void echo_service_init(void) {
  static char echo_service_str[BM_SERVICE_MAX_SERVICE_STRLEN];
  size_t topic_strlen = snprintf(echo_service_str, sizeof(echo_service_str), "%016" PRIx64 "%s",
                                 node_id(), echo_service_suffix);
  if (topic_strlen > 0) {
    bm_service_register(topic_strlen, echo_service_str, echo_service_handler);
  } else {
    printf("Failed to register echo service\n");
  }
}
