#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"
#include "task_priorities.h"
#include "timer_callback_handler.h"
#include "timers.h"
#include <stdarg.h>
#include <string.h>

#include "app_util.h"
#include "bcmp.h"
#include "bm_common_structs.h"
#include "bm_serial.h"
#include "bridgeLog.h"
#include "cbor.h"
#include "cbor_service_helper.h"
#include "config_cbor_map_service.h"
#include "config_cbor_map_srv_reply_msg.h"
#include "config_cbor_map_srv_request_msg.h"
#include "crc.h"
#include "device_info.h"
#include "messages.h"
#include "messages/info.h"
#include "messages/neighbors.h"
#include "network_config_logger.h"
#include "sensorController.h"
#include "sm_config_crc_list.h"
#include "stm32_rtc.h"
#include "sys_info_service.h"
#include "sys_info_svc_reply_msg.h"
#include "topology.h"
#include "topology_sampler.h"
#include "util.h"

#define TOPOLOGY_TIMEOUT_MS 60000
#define NETWORK_CONFIG_TIMEOUT_MS 1000

#define TOPOLOGY_LOADING_TIMEOUT_MS 60000
#define TOPOLOGY_BEGIN_TIMEOUT_MS 30

#define BUS_POWER_ON_DELAY 5000
#define NODE_SYS_INFO_REQUEST_TIMEOUT_S (3)
#define NODE_NETWORK_SYS_INFO_REQUEST_TIMEOUT_MS (NODE_SYS_INFO_REQUEST_TIMEOUT_S * 1000)
#define NODE_CONFIG_CBOR_MAP_REQUEST_TIMEOUT_S (3)
#define NODE_CONFIG_CBOR_MAP_REQUEST_TIMEOUT_MS (NODE_CONFIG_CBOR_MAP_REQUEST_TIMEOUT_S * 1000)
// Accounts for name of app + cbor config map + encoding inefficiencies
#define NODE_CONFIG_PADDING (512)

typedef struct network_configuration_info {
  uint32_t network_crc32;
  size_t cbor_config_map_size;
  uint8_t *cbor_config_map;
} network_configuration_info_s;

typedef struct node_list {
  uint64_t nodes[TOPOLOGY_SAMPLER_MAX_NODE_LIST_SIZE];
  abstractSensorType_e sensor_type[TOPOLOGY_SAMPLER_MAX_NODE_LIST_SIZE];
  uint16_t num_nodes;
  SemaphoreHandle_t node_list_mutex;
  network_configuration_info_s last_network_configuration_info;
} node_list_s;

static BridgePowerController *_bridge_power_controller;
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
static bool encode_sys_info(CborEncoder &array_encoder, SysInfoReplyData &sys_info);
static bool encode_cbor_configuration(CborEncoder &array_encoder,
                                      ConfigCborMapReplyData &cbor_map_reply);
static bool create_network_info_cbor_array(uint8_t *cbor_buffer, size_t &cbor_bufsize);

static void _update_sensor_type_list(uint64_t node_id, char *app_name, uint32_t app_name_len);

static void log_network_crc_info(uint32_t network_crc32, SMConfigCRCList &sm_config_crc_list);

static void topology_sample_cb(NetworkTopology *networkTopology) {
  uint8_t *cbor_buffer = NULL;
  BmNetworkInfo *network_info = NULL;
  xSemaphoreTake(_node_list.node_list_mutex, portMAX_DELAY);
  do {
    SMConfigCRCList sm_config_crc_list(BM_CFG_PARTITION_HARDWARE);
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
    memset(_node_list.sensor_type, 0, sizeof(_node_list.sensor_type));

    NeighborTableEntry *cursor = NULL;
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
        _node_list.num_nodes *
        (sizeof(SysInfoReplyData) + sizeof(ConfigCborMapReplyData) + NODE_CONFIG_PADDING);
    cbor_buffer = static_cast<uint8_t *>(pvPortMalloc(cbor_bufsize));
    configASSERT(cbor_buffer);
    memset(cbor_buffer, 0, cbor_bufsize);

    if (create_network_info_cbor_array(cbor_buffer, cbor_bufsize)) {
      network_crc32_calc = crc32_ieee(cbor_buffer, cbor_bufsize);
    } else {
      printf("Failed to create network info cbor array\n");
      break;
    }

    if (cbor_buffer) {
      for (uint32_t i = 0; i < cbor_bufsize; i++) {
        printf("%02x ", cbor_buffer[i]);
        if (i % 16 == 15) { // print a newline every 16 bytes (for print pretty-ness)
          printf("\n");
        }
      }
      // If we have a new configuration, store it.
      if (_node_list.last_network_configuration_info.network_crc32 != network_crc32_calc) {
        if (_node_list.last_network_configuration_info.cbor_config_map) {
          vPortFree(_node_list.last_network_configuration_info.cbor_config_map);
          _node_list.last_network_configuration_info.cbor_config_map = NULL;
        }
        _node_list.last_network_configuration_info.network_crc32 = network_crc32_calc;
        _node_list.last_network_configuration_info.cbor_config_map_size = cbor_bufsize;
        _node_list.last_network_configuration_info.cbor_config_map =
            static_cast<uint8_t *>(pvPortMalloc(cbor_bufsize));
        configASSERT(_node_list.last_network_configuration_info.cbor_config_map);
        memcpy(_node_list.last_network_configuration_info.cbor_config_map, cbor_buffer,
               cbor_bufsize);
        log_network_crc_info(network_crc32_calc, sm_config_crc_list);
        log_cbor_network_configurations(cbor_buffer, cbor_bufsize);
      }
      printf("\n");
    }

    bool known = sm_config_crc_list.contains(network_crc32_calc);
    if (!known || _send_on_boot) {
      if (!known) {
        bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_INFO, USE_HEADER,
                       "The smConfigurationCrc is not in the known list! calc: 0x%" PRIx32
                       " Adding it.\n",
                       network_crc32_calc);
        sm_config_crc_list.add(network_crc32_calc);
        if (!save_config(BM_CFG_PARTITION_HARDWARE, false)) {
          printf("Failed to save crc!\n");
        }
      }

      BmConfigCrc config_crc = {
          .partition = BM_CFG_PARTITION_SYSTEM,
          .crc32 = services_cbor_encoded_as_crc32(BM_CFG_PARTITION_SYSTEM),
      };

      BmFwVersion fw_info = {
          .major = 0,
          .minor = 0,
          .revision = 0,
          .gitSHA = 0,
      };

      getFWVersion(&fw_info.major, &fw_info.minor, &fw_info.revision);
      fw_info.gitSHA = getGitSHA();

      bm_serial_send_network_info(network_crc32_calc, &config_crc, &fw_info,
                                  _node_list.num_nodes, _node_list.nodes, cbor_bufsize,
                                  cbor_buffer);
      if (_send_on_boot) {
        _send_on_boot = false;
      }
    }

    // The first four inputs are not used by this message type
    reportBuilderAddToQueue(0, 0, NULL, 0, REPORT_BUILDER_CHECK_CRC);

  } while (0);
  xSemaphoreGive(_node_list.node_list_mutex);

  // Notify the sensor controller task to sample for sensors
  xTaskNotify(sensor_controller_task_handle, SAMPLER_TIMER_BITS, eSetBits);

  if (cbor_buffer) {
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
  SysInfoReplyData reply = {0, 0, 0, 0, NULL};
  printf("Service: %.*s\n", service_strlen, service);
  printf("Reply: %" PRIu32 "\n", msg_id);
  do {
    if (ack) {
      // Memory is allocated in the decode function, if we fail to decode, we need to free it.
      // Otherwise, we'll free it in the topology sampler task.
      if (sys_info_reply_decode(&reply, reply_data, reply_len) != CborNoError) {
        printf("Failed to decode sys info reply\n");
        break;
      }
      printf(" * Node id: %016" PRIx64 "\n", reply.node_id);
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
  } while (0);

  if (!rval && reply.app_name) {
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
  ConfigCborMapReplyData reply = {0, 0, 0, 0, NULL};
  printf("Service: %.*s\n", service_strlen, service);
  printf("Reply: %" PRIu32 "\n", msg_id);
  do {
    if (ack) {
      // Memory is allocated in the decode function, if we fail to decode, we need to free it.
      // Otherwise, we'll free it in the topology sampler task.
      if (config_cbor_map_reply_decode(&reply, reply_data, reply_len) != CborNoError) {
        printf("Failed to decode cbor map reply\n");
        break;
      }
      printf(" * Node id: %016" PRIx64 "\n", reply.node_id);
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
  } while (0);

  if (!rval && reply.cbor_data) {
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
    err = cbor_encoder_create_array(&encoder, &array_encoder, _node_list.num_nodes);
    if (err != CborNoError) {
      printf("cbor_encoder_create_array failed: %d\n", err);
      if (err != CborErrorOutOfMemory) {
        break;
      }
    }
    // Handle & count each reply coming back.
    uint32_t node_crc_rx_count = 0;
    SysInfoReplyData info_reply = {0, 0, 0, 0, NULL};
    ConfigCborMapReplyData cbor_map_reply = {0, 0, 0, 0, NULL};
    while (node_crc_rx_count < _node_list.num_nodes) {
      // For each node, create a sub-array.
      CborEncoder sub_array_encoder;
      err = cbor_encoder_create_array(&array_encoder, &sub_array_encoder,
                                      NUM_CONFIG_FIELDS_PER_NODE);
      if (err != CborNoError) {
        printf("cbor_encoder_create_array failed: %d\n", err);
        break;
      }
      // Update the network crc with all of the node crcs
      if (xQueueReceive(_sys_info_queue, &info_reply,
                        pdMS_TO_TICKS(NODE_NETWORK_SYS_INFO_REQUEST_TIMEOUT_MS))) {

        // This function will use the app name to determine the sensor type and update the _node_list.sensor_type array
        // so that the reportBuilder can pull the sensor type from the node id. The sensor type array must match the
        // order of the node id array. This function call is placed here since it is run from the context of the topology
        // sampler callback, which will already have the _node_list mutex taken.
        _update_sensor_type_list(info_reply.node_id, info_reply.app_name,
                                 info_reply.app_name_strlen);

        // If we have a sys info reply coming back, request the cbor map
        if (!config_cbor_map_service_request(
                info_reply.node_id, CONFIG_CBOR_MAP_PARTITION_ID_SYS, cbor_config_map_reply_cb,
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
      if (info_reply.app_name) {
        vPortFree(info_reply.app_name);
        info_reply.app_name = NULL;
      }
      if (cbor_map_reply.cbor_data) {
        vPortFree(cbor_map_reply.cbor_data);
        cbor_map_reply.cbor_data = NULL;
      }
    }
    // Free the memory allocated in the decode functions in case we broke out of the loop.
    if (info_reply.app_name) {
      vPortFree(info_reply.app_name);
    }
    if (cbor_map_reply.cbor_data) {
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
static bool encode_sys_info(CborEncoder &array_encoder, SysInfoReplyData &sys_info) {
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
                                      ConfigCborMapReplyData &cbor_map_reply) {
  CborParser parser;
  CborValue map;
  CborError err;
  do {
    // Open and validate the received cbor map.
    err = cbor_parser_init(cbor_map_reply.cbor_data, cbor_map_reply.cbor_encoded_map_len, 0,
                           &parser, &map);
    if (err != CborNoError) {
      break;
    }
    err = cbor_value_validate_basic(&map);
    if (err != CborNoError) {
      break;
    }
    if (!cbor_value_is_map(&map)) {
      err = CborErrorIllegalType;
      break;
    }
    memcpy(array_encoder.data.ptr, cbor_map_reply.cbor_data,
           cbor_map_reply.cbor_encoded_map_len);
    array_encoder.data.ptr += cbor_map_reply.cbor_encoded_map_len;
    if (array_encoder.remaining) {
      array_encoder.remaining--;
    }
  } while (0);

  printf("encode_cbor_configuration: %d\n", err);
  return err == CborNoError;
}

void topology_sampler_init(BridgePowerController *power_controller) {
  // TODO - add unit tests with mocking timer callbacks
  configASSERT(power_controller);
  _bridge_power_controller = power_controller;
  _sampling_enabled = false;
  _send_on_boot = true;
  int tmr_id = 2;
  _node_list.node_list_mutex = xSemaphoreCreateMutex();
  configASSERT(_node_list.node_list_mutex);
  topology_timer = xTimerCreate("Topology timer", (TOPOLOGY_TIMEOUT_MS / portTICK_RATE_MS),
                                pdTRUE, (void *)&tmr_id, topology_timer_handler);
  configASSERT(topology_timer);
  _sys_info_queue = xQueueCreate(TOPOLOGY_SAMPLER_MAX_NODE_LIST_SIZE, sizeof(SysInfoReplyData));
  configASSERT(_sys_info_queue);
  _config_cbor_map_queue =
      xQueueCreate(TOPOLOGY_SAMPLER_MAX_NODE_LIST_SIZE, sizeof(ConfigCborMapReplyData));
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

/*!
  * @brief Get the sensor type list from the topology sampler
  * @param[out] sensor_type_list The sensor type list to populate
  * @param[in,out] sensor_type_list_size The size of the sensor type list in bytes. Will be updated with the size of the sensor type list in bytes.
  * @param[out] num_nodes The number of nodes in the node list.
  * @param[in] timeout_ms The timeout in milliseconds.
 */
bool topology_sampler_get_sensor_type_list(abstractSensorType_e *sensor_type_list,
                                           size_t &sensor_type_list_size, uint32_t &num_nodes,
                                           uint32_t timeout_ms) {
  configASSERT(sensor_type_list);
  bool rval = false;
  if (xSemaphoreTake(_node_list.node_list_mutex, pdMS_TO_TICKS(timeout_ms))) {
    do {
      if (!_node_list.num_nodes) {
        break;
      }
      if (sensor_type_list_size < _node_list.num_nodes * sizeof(abstractSensorType_e)) {
        break;
      }
      num_nodes = _node_list.num_nodes;
      sensor_type_list_size = _node_list.num_nodes * sizeof(abstractSensorType_e);
      memcpy(sensor_type_list, _node_list.sensor_type, sensor_type_list_size);
      rval = true;
    } while (0);
    xSemaphoreGive(_node_list.node_list_mutex);
  }
  return rval;
}

/*!
 * @brief Get the node position in the node list from the topology sampler
 * @param[in] node_id The node id to search for.
 * @param[in] timeout_ms The timeout in milliseconds.
 * @return The position of the node in the node list or -1 if not found.
 */
int8_t topology_sampler_get_node_position(uint64_t node_id, uint32_t timeout_ms) {
  int8_t requested_nodes_position = -1;
  if (xSemaphoreTake(_node_list.node_list_mutex, pdMS_TO_TICKS(timeout_ms))) {
    do {
      for (uint8_t i = 0; i < TOPOLOGY_SAMPLER_MAX_NODE_LIST_SIZE; i++) {
        if (_node_list.nodes[i] == node_id) {
          requested_nodes_position = i;
          break;
        }
      }
    } while (0);
    xSemaphoreGive(_node_list.node_list_mutex);
  } else {
    printf("Failed to take node list mutex\n");
  }
  return requested_nodes_position;
}

/*!
 * @brief Get the last network configuration from the topology sampler
 * @note This function will allocate memory for the cbor config map, the caller is responsible for freeing it.
 * @param[out] network_crc32 The network crc32
 * @param[out] cbor_config_size The size of the cbor config map in bytes.
 * @return The cbor config map or NULL if unable to retreive.
 */
uint8_t *topology_sampler_alloc_last_network_config(uint32_t &network_crc32,
                                                    uint32_t &cbor_config_size) {
  uint8_t *rval = NULL;
  if (xSemaphoreTake(_node_list.node_list_mutex, pdMS_TO_TICKS(NETWORK_CONFIG_TIMEOUT_MS))) {
    do {
      if (!_node_list.last_network_configuration_info.cbor_config_map) {
        break;
      }
      network_crc32 = _node_list.last_network_configuration_info.network_crc32;
      cbor_config_size = _node_list.last_network_configuration_info.cbor_config_map_size;
      rval = static_cast<uint8_t *>(pvPortMalloc(cbor_config_size));
      configASSERT(rval);
      memcpy(rval, _node_list.last_network_configuration_info.cbor_config_map,
             cbor_config_size);
    } while (0);
    xSemaphoreGive(_node_list.node_list_mutex);
  }
  return rval;
}

/*
 * @brief Callback function for "spotter/request-last-network-config" topic.
 * Triggers a sending of most recent network info.
 */
void bm_topology_last_network_info_cb(void) {
  if (xSemaphoreTake(_node_list.node_list_mutex, pdMS_TO_TICKS(NETWORK_CONFIG_TIMEOUT_MS))) {
    do {

      BmConfigCrc config_crc = {
          .partition = BM_CFG_PARTITION_SYSTEM,
          .crc32 = services_cbor_encoded_as_crc32(BM_CFG_PARTITION_SYSTEM),
      };

      BmFwVersion fw_info = {
          .major = 0,
          .minor = 0,
          .revision = 0,
          .gitSHA = 0,
      };

      getFWVersion(&fw_info.major, &fw_info.minor, &fw_info.revision);
      fw_info.gitSHA = getGitSHA();
      bm_serial_send_network_info(
          _node_list.last_network_configuration_info.network_crc32, &config_crc, &fw_info,
          _node_list.num_nodes, _node_list.nodes,
          _node_list.last_network_configuration_info.cbor_config_map_size,
          _node_list.last_network_configuration_info.cbor_config_map);
    } while (0);
    xSemaphoreGive(_node_list.node_list_mutex);
  }
}

/**
 * @brief Updates the sensor type list with the given node ID and application name.
 *
 * This function iterates over the node list. If a node with the given ID is found,
 * the function checks the application name. If the application name is "aanderaa",
 * the sensor type for that node is set to SENSOR_TYPE_AANDERAA. If the application
 * name is "bm_soft_module", the sensor type for that node is set to SENSOR_TYPE_SOFT.
 * This function makes sure that the sensor type list is updated in the same order as
 * the node list. By default the sensor type list is initialized to SENSOR_TYPE_UNKNOWN,
 * aka zero's.
 *
 * Note: This function can only be called in a location that has taken the
 * _node_list.node_list_mutex.
 *
 * @param node_id The ID of the node to update.
 * @param app_name The name of the application associated with the node.
 * @param app_name_len The length of the application name.
 */
static void _update_sensor_type_list(uint64_t node_id, char *app_name, uint32_t app_name_len) {
  (void)app_name_len;
  for (uint8_t i = 0; i < TOPOLOGY_SAMPLER_MAX_NODE_LIST_SIZE; i++) {
    if (_node_list.nodes[i] == node_id) {
      if (strncmp(app_name, "aanderaa", strlen("aanderaa")) == 0) {
        _node_list.sensor_type[i] = SENSOR_TYPE_AANDERAA;
        break;
      } else if (strncmp(app_name, "bm_soft_module", strlen("bm_soft_module")) == 0) {
        _node_list.sensor_type[i] = SENSOR_TYPE_SOFT;
        break;
      } else if (strncmp(app_name, "bm_rbr", strlen("bm_rbr")) == 0) {
        _node_list.sensor_type[i] = SENSOR_TYPE_RBR_CODA;
      } else if (strncmp(app_name, "seapoint_turbidity", strlen("seapoint_turbidity")) == 0) {
        _node_list.sensor_type[i] = SENSOR_TYPE_SEAPOINT_TURBIDITY;
      }
    }
  }
}

static void log_network_crc_info(uint32_t network_crc32, SMConfigCRCList &sm_config_crc_list) {
  uint32_t epoch = 0;
  RTCTimeAndDate_t rtc;
  rtcGet(&rtc);
  epoch = rtcGetMicroSeconds(&rtc) / 1000000;

  bridgeLogPrint(BRIDGE_CFG, BM_COMMON_LOG_LEVEL_INFO, USE_HEADER,
                 "Network configuration change detected! crc: 0x%" PRIx32 " at UTC:%" PRIu32
                 "\n",
                 network_crc32, epoch);
  uint32_t num_stored_crcs = 0;
  uint32_t *stored_crcs = sm_config_crc_list.alloc_list(num_stored_crcs);
  bridgeLogPrint(BRIDGE_CFG, BM_COMMON_LOG_LEVEL_INFO, USE_HEADER, "Stored CRCs: \n");
  if (stored_crcs) {
    for (uint32_t i = 0; i < num_stored_crcs; i++) {
      bridgeLogPrint(BRIDGE_CFG, BM_COMMON_LOG_LEVEL_INFO, NO_HEADER, "0x%" PRIx32 "\n",
                     stored_crcs[i]);
    }
  }
  if (stored_crcs) {
    vPortFree(stored_crcs);
  }
}
