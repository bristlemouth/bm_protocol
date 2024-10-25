#include "sys_info_service.h"
extern "C" {
#include "bm_os.h"
#include "cbor_service_helper.h"
#include "device.h"
#include "bm_service.h"
}
#include "sys_info_svc_reply_msg.h"
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define sys_info_service_suffix "/sys_info"

static bool sys_info_service_handler(size_t service_strlen, const char *service,
                                     size_t req_data_len, uint8_t *req_data, size_t *buffer_len,
                                     uint8_t *reply_data);

/*!
 * @brief Initialize the sys info service. This service will respond to requests for the sys info with sys_info_service_data_s.
 * @param[in] sys_config The system configuration.
 */
void sys_info_service_init(void) {
  static char sys_info_service_str[BM_SERVICE_MAX_SERVICE_STRLEN];
  size_t topic_strlen = snprintf(sys_info_service_str, sizeof(sys_info_service_str),
                                 "%016" PRIx64 "%s", node_id(), sys_info_service_suffix);
  if (topic_strlen > 0) {
    bm_service_register(topic_strlen, sys_info_service_str, sys_info_service_handler);
  } else {
    printf("Failed to register sys info service\n");
  }
}

/*!
 * @brief Request the sys info from a target node.
 * @param[in] target_node_id The node id of the target node.
 * @param[in] reply_cb The callback function to be called when the reply is received. Reply will in format via sys_info_service_data_s.
 * @param[in] timeout_s The timeout in seconds.
 * @return True if the request was sent, false otherwise.
 */
bool sys_info_service_request(uint64_t target_node_id, BmServiceReplyCb reply_cb,
                            uint32_t timeout_s) {
  bool rval = false;
  char *target_service_str = (char *)bm_malloc(BM_SERVICE_MAX_SERVICE_STRLEN);
  if (target_service_str) {
    size_t target_service_strlen =
        snprintf(target_service_str, BM_SERVICE_MAX_SERVICE_STRLEN, "%016" PRIx64 "%s",
                 target_node_id, sys_info_service_suffix);
    if (target_service_strlen > 0) {
      rval = bm_service_request(target_service_strlen, target_service_str, 0, NULL, reply_cb,
                                timeout_s);
    }
  }
  bm_free(target_service_str);
  return rval;
}

static bool sys_info_service_handler(size_t service_strlen, const char *service,
                                     size_t req_data_len, uint8_t *req_data, size_t *buffer_len,
                                     uint8_t *reply_data) {
  (void)(req_data);
  bool rval = false;
  printf("Data received on service: %.*s\n", service_strlen, service);
  do {
    if (req_data_len != 0) {
      printf("Invalid data received on sys info service\n");
      break;
    }
#ifndef APP_NAME
#error "APP_NAME must be defined"
#endif // APP_NAME
    SysInfoReplyData d;
    d.app_name = const_cast<char *>(APP_NAME);
    d.app_name_strlen = strlen(APP_NAME);
    d.git_sha = git_sha();
    d.node_id = node_id();
    d.sys_config_crc = services_cbor_encoded_as_crc32(BM_CFG_PARTITION_SYSTEM);
    size_t encoded_len;
    // Will return CborErrorOutOfMemory if buffer_len is too small
    if (sys_info_reply_encode(&d, reply_data, *buffer_len, &encoded_len) != CborNoError) {
      printf("Failed to encode sys info service reply\n");
      break;
    }
    *buffer_len = encoded_len; // Pass back the encoded length
    rval = true;
  } while (0);

  return rval;
}
