#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include "echo_service.h"
#include "bm_service.h"
#include "bm_service_common.h"
#include "device_info.h"

#define ECHO_SERVICE_SUFFIX "/echo"

static bool echo_service_handler (size_t service_strlen, 
                          const char * service, 
                          size_t req_data_len, 
                          uint8_t * req_data,
                          size_t &reply_len,
                          uint8_t * reply_data);

/*!
 * @brief Initialize the echo service.
 */
void echo_service_init(void) {
    static char echo_service_str[BM_SERVICE_MAX_SERVICE_STRLEN];
    size_t topic_strlen = snprintf(echo_service_str, sizeof(echo_service_str), "%" PRIx64 "%s", getNodeId(),
            ECHO_SERVICE_SUFFIX);
    if(topic_strlen > 0) {
        bm_service_register(topic_strlen, echo_service_str, echo_service_handler);
    } else {
        printf("Failed to register echo service\n");
    }
}

static bool echo_service_handler (size_t service_strlen, 
                          const char * service, 
                          size_t req_data_len, 
                          uint8_t * req_data,
                          size_t &reply_len,
                          uint8_t * reply_data) {
  bool rval = true;
  printf("Data received on service: %.*s\n", service_strlen, service);
  if(reply_len <= MAX_BM_SERVICE_DATA_SIZE){
    reply_len = req_data_len;
    memcpy(reply_data, req_data, req_data_len);
  } else {
    reply_len = 0;
    rval = false;
  }
  return rval;
}
