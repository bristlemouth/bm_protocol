#include "sys_info_service.h"
#include "FreeRTOS.h"
#include "bm_service.h"
#include "bm_service_common.h"
#include "device_info.h"
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

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
                                 "%" PRIx64 "%s", getNodeId(), SYS_INFO_SERVICE_SUFFIX);
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
      snprintf(target_service_str, BM_SERVICE_MAX_SERVICE_STRLEN, "%" PRIx64 "%s",
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
  size_t max_data_len = buffer_len;
  buffer_len = 0;
  printf("Data received on service: %.*s\n", service_strlen, service);
  do {
    if (req_data_len != 0) {
      printf("Invalid data received on sys info service\n");
      break;
    }
#ifndef APP_NAME
#error "APP_NAME must be defined"
#endif // APP_NAME
    size_t app_name_strlen = strlen(APP_NAME);
    const char *app_name = APP_NAME;
    size_t data_len = sizeof(sys_info_service_data_s) + app_name_strlen;
    if (data_len > max_data_len) {
      printf("Data length too large\n");
      break;
    }
    buffer_len = data_len;
    sys_info_service_data_s *sys_info_service_data =
        reinterpret_cast<sys_info_service_data_s *>(reply_data);
    sys_info_service_data->node_id = getNodeId();
    sys_info_service_data->gitSHA = getGitSHA();
    sys_info_service_data->sys_config_crc = _sys_config->getCRC32();
    sys_info_service_data->app_name_strlen = app_name_strlen;
    if (app_name_strlen > 0) {
      memcpy(sys_info_service_data->app_name, app_name, app_name_strlen);
    }
    rval = true;
  } while (0);

  return rval;
}
