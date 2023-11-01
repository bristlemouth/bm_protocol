#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"
#include "task_priorities.h"
#include "timer_callback_handler.h"
#include "timers.h"
#include <string.h>

#include "bcmp.h"
#include "bcmp_info.h"
#include "bcmp_messages.h"
#include "bcmp_neighbors.h"
#include "bcmp_topology.h"
#include "bm_common_structs.h"
#include "bm_l2.h"
#include "bm_serial.h"
#include "bm_util.h"
#include "cbor.h"
#include "config_cbor_map_service.h"
#include "config_cbor_map_srv_reply_msg.h"
#include "config_cbor_map_srv_request_msg.h"
#include "crc.h"
#include "device_info.h"
#include "sys_info_service.h"
#include "sys_info_svc_reply_msg.h"
#include "topology_sampler.h"
#include "util.h"

#define TOPOLOGY_TIMEOUT_MS 60000

#define TOPOLOGY_LOADING_TIMEOUT_MS 60000
#define TOPOLOGY_BEGIN_TIMEOUT_MS 30

#define BUS_POWER_ON_DELAY 5000
#define NODE_SYS_INFO_REQUEST_TIMEOUT_S (3)
#define NODE_NETWORK_SYS_INFO_REQUEST_TIMEOUT_MS (NODE_SYS_INFO_REQUEST_TIMEOUT_S * 1000)
#define NODE_CONFIG_CBOR_MAP_REQUEST_TIMEOUT_S (3)
#define NODE_CONFIG_CBOR_MAP_REQUEST_TIMEOUT_MS (NODE_CONFIG_CBOR_MAP_REQUEST_TIMEOUT_S * 1000)
 // Accounts for name of app + cbor config map + encoding inefficiencies
#define NODE_CONFIG_PADDING (512)
#define NUM_CONFIG_FIELDS_PER_NODE (5)

typedef struct node_list {
  uint64_t nodes[TOPOLOGY_SAMPLER_MAX_NODE_LIST_SIZE];
  uint16_t num_nodes;
  SemaphoreHandle_t node_list_mutex;
} node_list_s;

static BridgePowerController *_bridge_power_controller;
static cfg::Configuration *_hw_cfg;
static cfg::Configuration *_sys_cfg;
static TimerHandle_t topology_timer;
static bool _sampling_enabled;
static bool _send_on_boot;
static node_list_s _node_list;
static QueueHandle_t _sys_info_queue;
static QueueHandle_t _config_cbor_map_queue;

static void topology_timer_handler(TimerHandle_t tmr);
static void topology_sampler_task(void *parameters);
static bool sys_info_reply_cb(bool ack, uint32_t msg_id, size_t service_strlen,
                              const char *service, size_t reply_len, uint8_t *reply_data);
static bool cbor_config_map_reply_cb(bool ack, uint32_t msg_id, size_t service_strlen,
                                     const char *service, size_t reply_len,
                                     uint8_t *reply_data);
static bool encode_sys_info(CborEncoder &array_encoder,
                                SysInfoSvcReplyMsg::Data &sys_info);
static bool encode_cbor_configuration(CborEncoder &array_encoder,
                                      ConfigCborMapSrvReplyMsg::Data &cbor_map_reply);
static bool create_network_info_cbor_array(uint8_t *cbor_buffer, size_t &cbor_bufsize);

static void topology_sample_cb(networkTopology_t *networkTopology) {
  uint8_t *cbor_buffer = NULL;
  bm_common_network_info_t *network_info = NULL;
  xSemaphoreTake(_node_list.node_list_mutex, portMAX_DELAY);
  do {
    if (!networkTopology) {
      printf("networkTopology NULL, task must be busy\n");
      break;
    }
    // check to make sure bus was powered for the whole topo request
    // break if bus was powered down at some point
    if (!_bridge_power_controller->isBridgePowerOn()) {
      break;
    }

    uint32_t network_crc32_calc = 0;

    // compile all additional info here
    _node_list.num_nodes = networkTopology->length;
    // if we hit this, expand the node list size
    configASSERT(sizeof(_node_list.nodes) >= _node_list.num_nodes * sizeof(uint64_t));

    memset(_node_list.nodes, 0, sizeof(_node_list.nodes));

    neighborTableEntry_t *cursor = NULL;
    uint16_t counter;
    xQueueReset(_sys_info_queue);
    xQueueReset(_config_cbor_map_queue);
    bool exit = false;

    // Iterate through the entire list of nodes and request the sys info.
    for (cursor = networkTopology->front, counter = 0;
         (cursor != NULL) && (counter < _node_list.num_nodes);
         cursor = cursor->nextNode, counter++) {
      _node_list.nodes[counter] = cursor->neighbor_table_reply->node_id;
      if (!sys_info_service_request(_node_list.nodes[counter], sys_info_reply_cb,
                                    NODE_SYS_INFO_REQUEST_TIMEOUT_S)) {
        printf("Failed to send sys info request to node: %" PRIu64 "\n",
               _node_list.nodes[counter]);
        exit = true;
        break;
      }
    }
    if (exit) {
      break;
    }

    // Create the network info cbor array and calculate the crc32
    size_t cbor_bufsize =
        _node_list.num_nodes * (sizeof(SysInfoSvcReplyMsg::Data) +
                                sizeof(ConfigCborMapSrvReplyMsg::Data) + NODE_CONFIG_PADDING);
    cbor_buffer = static_cast<uint8_t *>(pvPortMalloc(cbor_bufsize));
    configASSERT(cbor_buffer);

    if (create_network_info_cbor_array(cbor_buffer, cbor_bufsize)) {
      network_crc32_calc = crc32_ieee(cbor_buffer, cbor_bufsize);
    } else {
      printf("Failed to create network info cbor array\n");
      break;
    }

    if (cbor_buffer) {
      // TODO: Printing the buffer for now, but we should send this to spotter before freeing. 
      for(uint32_t i = 0; i < cbor_bufsize; i++) {
        printf("%02x ", cbor_buffer[i]);
        if(i % 16 == 15) { // print a newline every 16 bytes (for print pretty-ness)
          printf("\n");
        }
      }
      printf("\n");
    }

    uint32_t network_crc32_stored = 0;
    _hw_cfg->getConfig(
        "smConfigurationCrc", strlen("smConfigurationCrc"),
        network_crc32_stored); // TODO - make this name consistent across the message type + config value

    // check the calculated crc with the one that is in the hw partition
    if (network_crc32_calc != network_crc32_stored || _send_on_boot) {
      if (network_crc32_calc != network_crc32_stored) {
        printf("The smConfigurationCrc and calculated one do not match! calc: 0x%" PRIx32
               " stored: 0x%" PRIx32 "\n",
               network_crc32_calc, network_crc32_stored);
        if (!_hw_cfg->setConfig("smConfigurationCrc", strlen("smConfigurationCrc"),
                                network_crc32_calc)) {
          printf("Failed to set crc in hwcfg\n");
        }
        if (!_hw_cfg->saveConfig(false)) {
          printf("Failed to save crc!\n");
        }
      }

      bm_common_config_crc_t config_crc = {
          .partition = BM_COMMON_CFG_PARTITION_SYSTEM,
          .crc32 = _sys_cfg->getCborEncodedConfigurationCrc32(),
      };

      bm_common_fw_version_t fw_info = {
          .major = 0,
          .minor = 0,
          .revision = 0,
          .gitSHA = 0,
      };

      getFWVersion(&fw_info.major, &fw_info.minor, &fw_info.revision);
      fw_info.gitSHA = getGitSHA();

      bm_serial_send_network_info(network_crc32_calc, &config_crc, &fw_info,
                                  _node_list.num_nodes, _node_list.nodes);
      if (_send_on_boot) {
        _send_on_boot = false;
      }
    }
  } while (0);
  xSemaphoreGive(_node_list.node_list_mutex);

  if(cbor_buffer){
    vPortFree(cbor_buffer);
  }
  if (network_info) {
    vPortFree(network_info);
  }
}

static void topology_sample(void) { bcmp_topology_start(topology_sample_cb); }

/**
 * @brief  Hook to begin a topology sample on the timer_callback_handler thread
 *
 * @note Begins a topology sample process.
 *
 * @param *arg
 * @return none
 */
static void topology_timer_cb(void *arg) {
  (void)arg;
  topology_sample();
}

/**
 * @brief Topology Timer Handler function
 *
 * @note Puts topology sample event onto the timer_callback_handler thread
 *
 * @param *tmr    Pointer to Timer struct
 * @return none
 */
static void topology_timer_handler(TimerHandle_t tmr) {
  (void)tmr;
  timer_callback_handler_send_cb(topology_timer_cb, NULL, 0);
}

/*!
 * @brief Callback function for sys info reply, sends to queue for processing.
 * 
 * @param[in] ack True if the message was acknowledged, false otherwise.
 * @param[in] msg_id The message id.
 * @param[in] service_strlen The length of the service string.
 * @param[in] service The service string.
 * @param[in] reply_len The length of the reply data.
 * @param[in] reply_data The reply data.
 * @return True if the request was handled, false otherwise.
 */
static bool sys_info_reply_cb(bool ack, uint32_t msg_id, size_t service_strlen,
                              const char *service, size_t reply_len, uint8_t *reply_data) {
  bool rval = false;
  SysInfoSvcReplyMsg::Data reply = {0, 0, 0, 0, NULL};
  printf("Service: %.*s\n", service_strlen, service);
  printf("Reply: %" PRIu32 "\n", msg_id);
  do {
    if (ack) {
      // Memory is allocated in the decode function, if we fail to decode, we need to free it.
      // Otherwise, we'll free it in the topology sampler task.
      if (SysInfoSvcReplyMsg::decode(reply, reply_data, reply_len) != CborNoError) {
        printf("Failed to decode sys info reply\n");
        break;
      }
      printf(" * Node id: %" PRIx64 "\n", reply.node_id);
      printf(" * Git SHA: 0x%08" PRIx32 "\n", reply.git_sha);
      printf(" * Sys config CRC: 0x%08" PRIx32 "\n", reply.sys_config_crc);
      printf(" * App name: %.*s\n", reply.app_name_strlen, reply.app_name);
      if (xQueueSend(_sys_info_queue, &reply, 0) != pdTRUE) {
        printf("Failed to send sys info\n");
        break;
      }
    } else {
      printf("NACK\n");
    }
    rval = true;
  } while(0);

  if(!rval && reply.app_name){
    vPortFree(reply.app_name);
  }
  return rval;
}

/*!
 * @brief Callback function for cbor map reply, sends to queue for processing.
 * 
 * @param[in] ack True if the message was acknowledged, false otherwise.
 * @param[in] msg_id The message id.
 * @param[in] service_strlen The length of the service string.
 * @param[in] service The service string.
 * @param[in] reply_len The length of the reply data.
 * @param[in] reply_data The reply data.
 * @return True if the request was handled, false otherwise.
 */
static bool cbor_config_map_reply_cb(bool ack, uint32_t msg_id, size_t service_strlen,
                                     const char *service, size_t reply_len,
                                     uint8_t *reply_data) {
  bool rval = false;
  ConfigCborMapSrvReplyMsg::Data reply = {0, 0, 0, 0, NULL};           
  printf("Service: %.*s\n", service_strlen, service);
  printf("Reply: %" PRIu32 "\n", msg_id);
  do {
    if (ack) {
      // Memory is allocated in the decode function, if we fail to decode, we need to free it.
      // Otherwise, we'll free it in the topology sampler task.
      if (ConfigCborMapSrvReplyMsg::decode(reply, reply_data, reply_len) != CborNoError) {
        printf("Failed to decode cbor map reply\n");
        break;
      }
      printf(" * Node id: %" PRIx64 "\n", reply.node_id);
      printf(" * Partition: %" PRId32 "\n", reply.partition_id);
      printf(" * Cbor map len: %" PRIu32 "\n", reply.cbor_encoded_map_len);
      printf(" * Success: %" PRId32 "\n", reply.success);
      if (reply.success) {
        for (uint32_t i = 0; i < reply.cbor_encoded_map_len; i++) {
          if (!reply.cbor_data) {
            printf("NULL map\n");
            break;
          }
          printf("%02x ", reply.cbor_data[i]);
          if (i % 16 == 15) { // print a newline every 16 bytes (for print pretty-ness)
            printf("\n");
          }
        }
      }
      printf("\n");
      if (!xQueueSend(_config_cbor_map_queue, &reply, 0)) {
        printf("Failed to send cbor map\n");
        break;
      }
    } else {
      printf("NACK\n");
    }
    rval = true;
  } while(0);

  if(!rval && reply.cbor_data) {
    vPortFree(reply.cbor_data);
  }

  return rval;
}

/*!
 * @brief Creates the 2D Cbor array of the network configuration
 * 
 * @param[in/out] cbor_buffer The buffer to store the cbor array in.
 * @param[in/out] cbor_bufsize The size of the buffer passed in, on out it's the encoded cbor length.
 * @return True if the cbor array was created, false otherwise.
 */
static bool create_network_info_cbor_array(uint8_t *cbor_buffer, size_t &cbor_bufsize) {
  bool rval = false;
  do {
    // Init the top level Cbor Array
    CborError err;
    CborEncoder encoder, array_encoder;
    cbor_encoder_init(&encoder, cbor_buffer, cbor_bufsize, 0);
    err = cbor_encoder_create_array(&encoder, &array_encoder,
                                    _node_list.num_nodes);
    if (err != CborNoError) {
      printf("cbor_encoder_create_array failed: %d\n", err);
      if (err != CborErrorOutOfMemory) {
        break;
      }
    }
    // Handle & count each reply coming back.
    uint32_t node_crc_rx_count = 0;
    SysInfoSvcReplyMsg::Data info_reply = {0, 0, 0, 0, NULL};
    ConfigCborMapSrvReplyMsg::Data cbor_map_reply = {0, 0, 0, 0, NULL};
    while (node_crc_rx_count < _node_list.num_nodes) {
      // For each node, create a sub-array.
      CborEncoder sub_array_encoder;
      err = cbor_encoder_create_array(&array_encoder, &sub_array_encoder,
                                      NUM_CONFIG_FIELDS_PER_NODE);
      if(err != CborNoError) {
        printf("cbor_encoder_create_array failed: %d\n", err);
        break;
      }
      // Update the network crc with all of the node crcs
      if (xQueueReceive(_sys_info_queue, &info_reply,
                        pdMS_TO_TICKS(NODE_NETWORK_SYS_INFO_REQUEST_TIMEOUT_MS))) {
        // If we have a sys info reply coming back, request the cbor map
        if (!config_cbor_map_service_request(info_reply.node_id, CONFIG_CBOR_MAP_PARTITION_ID_SYS,
                                           cbor_config_map_reply_cb,
                                           NODE_CONFIG_CBOR_MAP_REQUEST_TIMEOUT_S)) {
          printf("Failed to request cbor map\n");
          break;
        }
        // Encode the sys info first.
        if (!encode_sys_info(sub_array_encoder, info_reply)) {
          printf("Failed to encode network info\n");
          break;
        }
        // Wait for the Cbor Map reply to arrive.
        if (xQueueReceive(_config_cbor_map_queue, &cbor_map_reply,
                          NODE_CONFIG_CBOR_MAP_REQUEST_TIMEOUT_MS)) {
          // Now encode the cbor configuration map reply.
          if (!encode_cbor_configuration(sub_array_encoder, cbor_map_reply)) {
            printf("Failed to encode config info\n");
            break;
          }
          // Now we're finished with this node, close the sub-array.
          err = cbor_encoder_close_container(&array_encoder, &sub_array_encoder);
          if (err != CborNoError) {
            printf("cbor_encoder_close_container failed: %d\n", err);
            break;
          }
        } else {
          printf("Failed to receive cbor map reply\n");
          break;
        }
        node_crc_rx_count++;

      } else {
        printf("Failed to receive node sys info\n");
        break;
      }
      // Free the memory allocated in the decode functions every loop.
      if(info_reply.app_name) {
        vPortFree(info_reply.app_name);
        info_reply.app_name = NULL;
      }
      if(cbor_map_reply.cbor_data) {
        vPortFree(cbor_map_reply.cbor_data);
        cbor_map_reply.cbor_data = NULL;
      }
    }
    // Free the memory allocated in the decode functions in case we broke out of the loop.
    if(info_reply.app_name) {
      vPortFree(info_reply.app_name);
    }
    if(cbor_map_reply.cbor_data) {
      vPortFree(cbor_map_reply.cbor_data);
    }

    // Check if we've processed all expected nodes.
    if (node_crc_rx_count != _node_list.num_nodes) {
      printf("Failed to receive all info\n");
      break;
    } else {
      printf("Received all node info\n");
    }
    // Close the top-level array.
    err = cbor_encoder_close_container(&encoder, &array_encoder);
    if (err == CborNoError) {
      cbor_bufsize = cbor_encoder_get_buffer_size(&encoder, cbor_buffer);
      rval = true;
    } else {
      printf("cbor_encoder_close_container failed: %d\n", err);
      if (err != CborErrorOutOfMemory) {
        break;
      }
      size_t extra_bytes_needed = cbor_encoder_get_extra_bytes_needed(&encoder);
      printf("extra_bytes_needed: %zu\n", extra_bytes_needed);
    }
  } while (0);
  return rval;
}

/*!
 * @brief Encodes the sys info into the cbor array.
 *
 * @param[in/out] array_encoder The encoder to encode into.
 * @param[in] sys_info The sys info to encode.
 * @return True if the sys info was encoded, false otherwise.
 */
static bool encode_sys_info(CborEncoder &array_encoder,
                                SysInfoSvcReplyMsg::Data &sys_info) {
  CborError err = CborNoError;
  do {
    // node id
    err = cbor_encode_uint(&array_encoder, sys_info.node_id);
    if (err != CborNoError) {
      printf("Failed to encode node_id\n");
      break;
    }

    // app_name
    err = cbor_encode_text_string(&array_encoder, sys_info.app_name, sys_info.app_name_strlen);
    if (err != CborNoError) {
      printf("Failed to encode app_name\n");
      break;
    }

    // git sha
    err = cbor_encode_uint(&array_encoder, sys_info.git_sha);
    if (err != CborNoError) {
      printf("Failed to encode git_sha\n");
      break;
    }

    // sys config crc
    err = cbor_encode_uint(&array_encoder, sys_info.sys_config_crc);
    if (err != CborNoError) {
      printf("Failed to encode sys_config_crc\n");
      break;
    }
  } while (0);
  return (err == CborNoError);
}

/*!
 * @brief Encodes the cbor configuration map into the cbor array.
 *
 * @param[in/out] array_encoder The encoder to encode into.
 * @param[in] cbor_map_reply The cbor map reply to encode.
 * @return True if the cbor map reply was encoded, false otherwise.
 */
static bool encode_cbor_configuration(CborEncoder &array_encoder,
                                      ConfigCborMapSrvReplyMsg::Data &cbor_map_reply) {
  CborEncoder map_encoder;
  CborParser parser;
  CborValue map,it;
  CborError err;
  char * tmp_buf = NULL;
  do {
    // Open and validate the received cbor map.
    err = cbor_parser_init(cbor_map_reply.cbor_data, cbor_map_reply.cbor_encoded_map_len, 0, &parser, &map);
    if(err != CborNoError) {
      break;
    }
    err = cbor_value_validate_basic(&map);
    if (err != CborNoError) {
      break;
    }
    if(!cbor_value_is_map(&map)){
      err = CborErrorIllegalType;
      break;
    }
    size_t num_fields;
    err = cbor_value_get_map_length(&map, &num_fields);
    if (err != CborNoError) {
      break;
    }
    // Create a map in the subarray to encode to.
    err = cbor_encoder_create_map(&array_encoder, &map_encoder, num_fields);
    if(err != CborNoError) {
      break;
    }
    err = cbor_value_enter_container(&map, &it);
    if(err != CborNoError) {
      break;
    }
    size_t tmp_len;
    tmp_buf = static_cast<char *>(pvPortMalloc(cfg::MAX_CONFIG_BUFFER_SIZE_BYTES));
    configASSERT(tmp_buf);
    // Loop through all the key-value pairs in the cbor map and encode them to the subarray.
    for(size_t i = 0; i < num_fields; i++){
      // Get & encode key
      if (!cbor_value_is_text_string(&it)) {
        err = CborErrorIllegalType;
        printf("expected string key but got something else\n");
        break;
      }
      err = cbor_value_get_string_length(&it, &tmp_len);
      if(err != CborNoError) {
        break;
      }
      err = cbor_value_copy_text_string(&it, tmp_buf, &tmp_len, NULL);
      if(err != CborNoError) {
        break;
      }
      err = cbor_encode_text_string(&map_encoder, tmp_buf, tmp_len);
      if(err != CborNoError) {
        break;
      }
      err = cbor_value_advance(&it);
      if (err != CborNoError) {
        break;
      }
      // Get & encode value
      switch(cbor_value_get_type(&it)){
        case CborIntegerType:{
          if(cbor_value_is_unsigned_integer(&it)){
            uint64_t tmp;
            err = cbor_encode_uint(&map_encoder, cbor_value_get_uint64(&it, &tmp));
          } else {
            int64_t tmp;
            err = cbor_encode_int(&map_encoder, cbor_value_get_int64(&it, &tmp));
          }
          break;
        }
        case CborDoubleType:{
          double tmp;
          err = cbor_encode_double(&map_encoder, cbor_value_get_double(&it, &tmp));
          break;
        }
        case CborBooleanType:{
          bool tmp;
          err = cbor_encode_boolean(&map_encoder, cbor_value_get_boolean(&it, &tmp));
          break;
        }
        case CborFloatType:{
          float tmp;
          err = cbor_encode_float(&map_encoder, cbor_value_get_float(&it, &tmp));
          break;
        }
        case CborTextStringType:{
          err = cbor_value_get_string_length(&it, &tmp_len);
          if (err != CborNoError) {
            break;
          }
          err = cbor_value_copy_text_string(&it, tmp_buf, &tmp_len, NULL);
          if(err != CborNoError) {
            break;
          }
          err = cbor_encode_text_string(&map_encoder, tmp_buf, tmp_len);
          if(err != CborNoError) {
            break;
          }
          break;
        }
        case CborByteStringType:{
          err = cbor_value_get_string_length(&it, &tmp_len);
          if (err != CborNoError) {
            break;
          }
          err = cbor_value_copy_byte_string(&it, reinterpret_cast<uint8_t*>(tmp_buf), &tmp_len, NULL);
          if(err != CborNoError) {
            break;
          }
          err = cbor_encode_byte_string(&map_encoder, reinterpret_cast<uint8_t*>(tmp_buf), tmp_len);
          if(err != CborNoError) {
            break;
          }
          break;
        }
        default:{
          err = CborErrorIllegalType;
          break;
        }
      }
      if(err != CborNoError) {
        break;
      }
      err = cbor_value_advance(&it);
      if (err != CborNoError) {
        break;
      }
    }
    if(err != CborNoError) {
      break;
    }

    err = cbor_encoder_close_container(&array_encoder, &map_encoder);
    if(err != CborNoError) {
      break;
    }

    err = cbor_value_leave_container(&map, &it);
    if (err != CborNoError) {
      break;
    }
  
  } while(0);
  if(tmp_buf) {
    vPortFree(tmp_buf);
  }
  printf("encode_cbor_configuration: %d\n", err);
  return err == CborNoError;
}

void topology_sampler_init(BridgePowerController *power_controller, cfg::Configuration *hw_cfg,
                           cfg::Configuration *sys_cfg) {
  // TODO - add unit tests with mocking timer callbacks
  configASSERT(power_controller);
  _bridge_power_controller = power_controller;
  _hw_cfg = hw_cfg;
  _sys_cfg = sys_cfg;
  _sampling_enabled = false;
  _send_on_boot = true;
  int tmr_id = 2;
  _node_list.node_list_mutex = xSemaphoreCreateMutex();
  configASSERT(_node_list.node_list_mutex);
  topology_timer = xTimerCreate("Topology timer", (TOPOLOGY_TIMEOUT_MS / portTICK_RATE_MS),
                                pdTRUE, (void *)&tmr_id, topology_timer_handler);
  configASSERT(topology_timer);
  _sys_info_queue =
      xQueueCreate(TOPOLOGY_SAMPLER_MAX_NODE_LIST_SIZE, sizeof(SysInfoSvcReplyMsg::Data));
  configASSERT(_sys_info_queue);
  _config_cbor_map_queue =
      xQueueCreate(TOPOLOGY_SAMPLER_MAX_NODE_LIST_SIZE, sizeof(ConfigCborMapSrvReplyMsg::Data));
  configASSERT(_config_cbor_map_queue);
  // create task
  BaseType_t rval = xTaskCreate(topology_sampler_task, "TOPO_SAMPLER", 1024, NULL,
                                TOPO_SAMPLER_TASK_PRIORITY, NULL);
  configASSERT(rval == pdPASS);
}

void topology_sampler_task(void *parameters) {
  (void)parameters;
  // This task will wait forever for the bus to turn on, then when it does we will wait 5s and send a bcmp_topoloy start and start a timer for 1 min
  // then each time the timer hits we will do the timer callback, and when the power is turned off we will turn off the timer.
  // This timer will have to wait for the two mintue init period to elapse before beginning

  // lets wait 5 seconds for the devices on the bus to power up
  vTaskDelay(pdMS_TO_TICKS(BUS_POWER_ON_DELAY));
  // here we will do an initial sample during the bridges 2 minute on period
  topology_sample();

  for (;;) {
    // After getting the initial topology lets wait for the init period to
    // end so that we don't accidentally turn off the bus while doing
    // we are building our topology
    while (!_bridge_power_controller->initPeriodElapsed()) {
      vTaskDelay(pdMS_TO_TICKS(BUS_POWER_ON_DELAY));
    }

    // Wait for the power control to set the bus ON
    if (_bridge_power_controller->isPowerControlEnabled()) {
      // Check if we are already sampling, if not, wait (using blocking waitForSignal) for power to turn on
      // if we are sampling, wait for power to turn off
      if (!_sampling_enabled && _bridge_power_controller->waitForSignal(true, portMAX_DELAY)) {
        // lets wait 5 seconds for the devices on the bus to power up
        vTaskDelay(pdMS_TO_TICKS(BUS_POWER_ON_DELAY));
        topology_sample();
        // start the timer! here while the bus is powered we will sample topology every minute
        configASSERT(xTimerStart(topology_timer, 10));

        _sampling_enabled = true;

      } else if (_sampling_enabled &&
                 _bridge_power_controller->waitForSignal(false, portMAX_DELAY)) {
        _sampling_enabled = false;
        configASSERT(xTimerStop(topology_timer, 10));
      }
    } else {
      // still need to wait after init period for the bus to be turned back on
      _bridge_power_controller->waitForSignal(true, portMAX_DELAY);
      // lets wait 5 seconds to make sure the devices on the bus are powered on
      vTaskDelay(pdMS_TO_TICKS(BUS_POWER_ON_DELAY));
      topology_sample();
      // start the timer! here while the bus is powered we will sample topology every minute
      configASSERT(xTimerStart(topology_timer, 10));
      // If DFU disabled the power controller before sampling began, we may have gotten here
      // incorrectly, so lets just check to make sure the power controller is disabled. If it
      // is re-enabled, then we will just loop back to the beginning of the for loop
      while (!_bridge_power_controller->isPowerControlEnabled()) {
        vTaskDelay(pdMS_TO_TICKS(1000));
      }
      // We should stop the timer if the power controller is re-enabled
      configASSERT(xTimerStop(topology_timer, 10));
    }
  }
}

/*!
  * @brief Get the node list from the topology sampler
  * @param[out] node_list The node list to populate
  * @param[in,out] node_list_size The size of the node list in bytes. Will be updated with the size of the node list in bytes.
  * @param[out] num_nodes The number of nodes in the node list.
  * @param[in] timeout_ms The timeout in milliseconds.
 */
bool topology_sampler_get_node_list(uint64_t *node_list, size_t &node_list_size,
                                    uint32_t &num_nodes, uint32_t timeout_ms) {
  configASSERT(node_list);
  bool rval = false;
  if (xSemaphoreTake(_node_list.node_list_mutex, pdMS_TO_TICKS(timeout_ms))) {
    do {
      if (!_node_list.num_nodes) {
        break;
      }
      if (node_list_size < _node_list.num_nodes * sizeof(uint64_t)) {
        break;
      }
      num_nodes = _node_list.num_nodes;
      node_list_size = _node_list.num_nodes * sizeof(uint64_t);
      memcpy(node_list, _node_list.nodes, node_list_size);
      rval = true;
    } while (0);
    xSemaphoreGive(_node_list.node_list_mutex);
  }
  return rval;
}
