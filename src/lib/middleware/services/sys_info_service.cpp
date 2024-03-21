#include "sys_info_service.h"
#include "FreeRTOS.h"
#include "bm_service.h"
#include "bm_service_common.h"
#include "device_info.h"
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "sys_info_svc_reply_msg.h"

using namespace cfg;

static Configuration *_sys_config;

#define SYS_INFO_SERVICE_SUFFIX "/sys_info"

static bool sys_info_service_handler(size_t service_strlen, const char *service,
                                   size_t req_data_len, uint8_t *req_data, size_t &buffer_len,
                                   uint8_t *reply_data);

/*!
 * @brief Initialize the sys info service. This service will respond to requests for the sys info with sys_info_service_data_s.
 * @param[in] sys_config The system configuration.
 */
void sys_info_service_init(Configuration &sys_config) {
  _sys_config = &sys_config;
  static char sys_info_service_str[BM_SERVICE_MAX_SERVICE_STRLEN];
  size_t topic_strlen = snprintf(sys_info_service_str, sizeof(sys_info_service_str),
                                 "%016" PRIx64 "%s", getNodeId(), SYS_INFO_SERVICE_SUFFIX);
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
bool sys_info_service_request(uint64_t target_node_id, bm_service_reply_cb reply_cb,
                            uint32_t timeout_s) {
  bool rval = false;
  char *target_service_str = static_cast<char *>(pvPortMalloc(BM_SERVICE_MAX_SERVICE_STRLEN));
  configASSERT(target_service_str);
  size_t target_service_strlen =
      snprintf(target_service_str, BM_SERVICE_MAX_SERVICE_STRLEN, "%016" PRIx64 "%s",
               target_node_id, SYS_INFO_SERVICE_SUFFIX);
  if (target_service_strlen > 0) {
    rval = bm_service_request(target_service_strlen, target_service_str, 0, NULL, reply_cb,
                              timeout_s);
  }
  vPortFree(target_service_str);
  return rval;
}

static bool sys_info_service_handler(size_t service_strlen, const char *service,
                                   size_t req_data_len, uint8_t *req_data, size_t &buffer_len,
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
    SysInfoSvcReplyMsg::Data d;
    d.app_name = const_cast<char *>(APP_NAME);
    d.app_name_strlen = strlen(APP_NAME);
    d.git_sha = getGitSHA();
    d.node_id = getNodeId();
    d.sys_config_crc = _sys_config->getCborEncodedConfigurationCrc32();
    size_t encoded_len;
    // Will return CborErrorOutOfMemory if buffer_len is too small
    if(SysInfoSvcReplyMsg::encode(d, reply_data, buffer_len, &encoded_len) != CborNoError) {
      printf("Failed to encode sys info service reply\n");
      break;
    }
    buffer_len = encoded_len; // Pass back the encoded length
    rval = true;
  } while (0);

  return rval;
}
