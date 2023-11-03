#include "config_cbor_map_service.h"
#include "FreeRTOS.h"
#include "bm_service.h"
#include "bm_service_common.h"
#include "config_cbor_map_srv_reply_msg.h"
#include "config_cbor_map_srv_request_msg.h"
#include "device_info.h"

#define CONFIG_MAP_SUFFIX "/config_map"

cfg::Configuration* _sys_config;
cfg::Configuration* _hw_config;
cfg::Configuration* _usr_config;

static bool config_map_service_handler(size_t service_strlen, const char *service,
                                   size_t req_data_len, uint8_t *req_data, size_t &buffer_len,
                                   uint8_t *reply_data);

void config_cbor_map_service_init(cfg::Configuration& hw_cfg, cfg::Configuration& sys_config, cfg::Configuration& user_config) {
  _sys_config = &sys_config;
  _hw_config = &hw_cfg;
  _usr_config = &user_config;
  static char config_map_str[BM_SERVICE_MAX_SERVICE_STRLEN];
  size_t topic_strlen = snprintf(config_map_str, sizeof(config_map_str),
                                 "%" PRIx64 "%s", getNodeId(), CONFIG_MAP_SUFFIX);
  if (topic_strlen > 0) {
    bm_service_register(topic_strlen, config_map_str, config_map_service_handler);
  } else {
    printf("Failed to register sys info service\n");
  }
}

bool config_cbor_map_service_request(uint64_t target_node_id, uint32_t partition_id, bm_service_reply_cb reply_cb, uint32_t timeout_s) { 
  bool rval = false;
  char *target_service_str = static_cast<char *>(pvPortMalloc(BM_SERVICE_MAX_SERVICE_STRLEN));
  configASSERT(target_service_str);
  size_t target_service_strlen =
      snprintf(target_service_str, BM_SERVICE_MAX_SERVICE_STRLEN, "%" PRIx64 "%s",
               target_node_id, CONFIG_MAP_SUFFIX);
  ConfigCborMapSrvRequestMsg::Data req = {partition_id};
  static constexpr size_t cbor_buflen = sizeof(ConfigCborMapSrvRequestMsg::Data) + 25; // Abitrary padding.
  uint8_t * cbor_buffer = static_cast<uint8_t *>(pvPortMalloc(cbor_buflen));
  configASSERT(cbor_buffer);
  size_t req_data_len;
  do {
    if(ConfigCborMapSrvRequestMsg::encode(req, cbor_buffer, cbor_buflen, &req_data_len) != CborNoError) {
        printf("Failed to encode config map request\n");
        break;
    }
    if (target_service_strlen > 0) {
        rval = bm_service_request(target_service_strlen, target_service_str, req_data_len, cbor_buffer, reply_cb,
                                timeout_s);
    }
  } while(0);
  vPortFree(cbor_buffer);
  vPortFree(target_service_str);
  return rval;
}

static bool config_map_service_handler(size_t service_strlen, const char *service,
                                   size_t req_data_len, uint8_t *req_data, size_t &buffer_len,
                                   uint8_t *reply_data) {
    (void) service_strlen;
    (void) service;
    bool rval = false;
    uint8_t * cbor_map = NULL;
    printf("Data received on config map service\n");
    do {
        ConfigCborMapSrvRequestMsg::Data req;
        if(ConfigCborMapSrvRequestMsg::decode(req, req_data, req_data_len) != CborNoError) {
            printf("Failed to decode config map request\n");
            break;
        }
        cfg::Configuration* config = NULL;
        if(req.partition_id == CONFIG_CBOR_MAP_PARTITION_ID_SYS){
            config = _sys_config;
        } else if(req.partition_id == CONFIG_CBOR_MAP_PARTITION_ID_HW) {
            config = _hw_config;
        } else if(req.partition_id == CONFIG_CBOR_MAP_PARTITION_ID_USER) {
            config = _usr_config;
        } else {
            config = NULL;
            printf("Invalid partition id\n");
        }
        size_t buffer_size;
        if(config) {
            cbor_map = config->asCborMap(buffer_size);
        }
        ConfigCborMapSrvReplyMsg::Data reply = {0, 0, 0, 0, NULL};
        reply.node_id = getNodeId();
        reply.partition_id = req.partition_id;
        reply.cbor_data = cbor_map;
        reply.cbor_encoded_map_len = (reply.cbor_data) ? buffer_size : 0;
        reply.success = (reply.cbor_data) ? true : false;
        size_t encoded_len;
        if(ConfigCborMapSrvReplyMsg::encode(reply, reply_data, buffer_len, &encoded_len)!= CborNoError) {
            printf("Failed to encode config map reply\n");
            break;
        }
        buffer_len = encoded_len;
        rval = true;
    } while(0);
    if(cbor_map){
        vPortFree(cbor_map);
    }
    return rval;
}
