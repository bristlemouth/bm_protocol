#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"
#include "task_priorities.h"
#include "timer_callback_handler.h"
#include "semphr.h"

#include "bm_l2.h"
#include "bcmp.h"
#include "bcmp_info.h"
#include "bcmp_messages.h"
#include "bcmp_neighbors.h"
#include "bcmp_topology.h"
#include "bm_common_structs.h"
#include "bm_util.h"
#include "bm_serial.h"
#include "crc.h"
#include "device_info.h"
#include "topology_sampler.h"
#include "util.h"

#define TOPOLOGY_TIMEOUT_MS 60000

#define TOPOLOGY_LOADING_TIMEOUT_MS 60000
#define TOPOLOGY_BEGIN_TIMEOUT_MS 30

#define BUS_POWER_ON_DELAY 5000

typedef struct node_list {
  uint64_t nodes[TOPOLOGY_SAMPLER_MAX_NODE_LIST_SIZE];
  uint16_t num_nodes;
  SemaphoreHandle_t node_list_mutex;
} node_list_s; 

static BridgePowerController *_bridge_power_controller;
static cfg::Configuration* _hw_cfg;
static cfg::Configuration* _sys_cfg;
static TimerHandle_t topology_timer;
static bool _sampling_enabled;
static bool _send_on_boot;
static node_list_s _node_list;

static void topology_timer_handler(TimerHandle_t tmr);
static void topology_sampler_task(void *parameters);

static void topology_sample_cb(networkTopology_t* networkTopology) {

  bm_common_network_info_t* network_info = NULL;
  xSemaphoreTake(_node_list.node_list_mutex, portMAX_DELAY);
  do {
    if (!networkTopology){
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
    configASSERT(sizeof(_node_list.nodes) >= _node_list.num_nodes * sizeof(uint64_t)); // if we hit this, expand the node list size

    network_crc32_calc = crc32_ieee_update(network_crc32_calc, reinterpret_cast<uint8_t *>(&_node_list.num_nodes), sizeof(_node_list.num_nodes));

    memset(_node_list.nodes, 0, sizeof(_node_list.nodes));

    neighborTableEntry_t *cursor = NULL;
    uint16_t counter;
    for(cursor = networkTopology->front, counter = 0; (cursor != NULL) && (counter < _node_list.num_nodes); cursor = cursor->nextNode, counter++) {
      _node_list.nodes[counter] = cursor->neighbor_table_reply->node_id;
      network_crc32_calc = crc32_ieee_update(network_crc32_calc, reinterpret_cast<uint8_t *>(&_node_list.nodes[counter]), sizeof(uint64_t));
    }

    bm_common_config_crc_t config_crc = {
      .partition = BM_COMMON_CFG_PARTITION_SYSTEM,
      .crc32 = _sys_cfg->getCRC32(),
    };

    network_crc32_calc = crc32_ieee_update(network_crc32_calc, reinterpret_cast<uint8_t *>(&config_crc), sizeof(bm_common_config_crc_t));

    bm_common_fw_version_t fw_info = {
      .major = 0,
      .minor = 0,
      .revision = 0,
      .gitSHA = 0,
    };

    getFWVersion(&fw_info.major, &fw_info.minor, &fw_info.revision);
    fw_info.gitSHA = getGitSHA();

    network_crc32_calc = crc32_ieee_update(network_crc32_calc, reinterpret_cast<uint8_t *>(&fw_info), sizeof(bm_common_fw_version_t));

    uint32_t network_crc32_stored = 0;
    _hw_cfg->getConfig("smConfigurationCrc", strlen("smConfigurationCrc"), network_crc32_stored); // TODO - make this name consistent across the message type + config value

    // check the calculated crc with the one that is in the hw partition
    if (network_crc32_calc != network_crc32_stored || _send_on_boot) {
      if (network_crc32_calc != network_crc32_stored) {
        printf("The smConfigurationCrc and calculated one do not match! calc: 0x%" PRIx32 " stored: 0x%" PRIx32 "\n", network_crc32_calc, network_crc32_stored);
        if(!_hw_cfg->setConfig("smConfigurationCrc", strlen("smConfigurationCrc"), network_crc32_calc)) {
          printf("Failed to set crc in hwcfg\n");
        }
        if(!_hw_cfg->saveConfig(false)) {
          printf("Failed to save crc!\n");
        }
      }
      bm_serial_send_network_info(network_crc32_calc, &config_crc, &fw_info, _node_list.num_nodes, _node_list.nodes);
      if (_send_on_boot) {
        _send_on_boot = false;
      }
    }
  } while (0);
  xSemaphoreGive(_node_list.node_list_mutex);

  if (network_info) {
    vPortFree(network_info);
  }
}

static void topology_sample(void) {
  bcmp_topology_start(topology_sample_cb);
}

/**
 * @brief  Hook to begin a topology sample on the timer_callback_handler thread
 *
 * @note Begins a topology sample process.
 *
 * @param *arg
 * @return none
 */
static void topology_timer_cb(void *arg) {
    (void) arg;
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
    (void) tmr;
    timer_callback_handler_send_cb(topology_timer_cb, NULL, 0);
}


void topology_sampler_init(BridgePowerController *power_controller, cfg::Configuration* hw_cfg, cfg::Configuration* sys_cfg) {
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
                                  pdTRUE, (void *) &tmr_id, topology_timer_handler);
  configASSERT(topology_timer);

  // create task
  BaseType_t rval = xTaskCreate(topology_sampler_task,
                                "TOPO_SAMPLER",
                                1024,
                                NULL,
                                TOPO_SAMPLER_TASK_PRIORITY,
                                NULL);
  configASSERT(rval == pdPASS);

}

void topology_sampler_task(void *parameters) {
  (void) parameters;
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
    while(!_bridge_power_controller->initPeriodElapsed()) {
      vTaskDelay(pdMS_TO_TICKS(BUS_POWER_ON_DELAY));
    }

    // Wait for the power control to set the bus ON
    if(_bridge_power_controller->isPowerControlEnabled()) {
      // Check if we are already sampling, if not, wait (using blocking waitForSignal) for power to turn on
      // if we are sampling, wait for power to turn off
      if (!_sampling_enabled && _bridge_power_controller->waitForSignal(true, portMAX_DELAY)) {
        // lets wait 5 seconds for the devices on the bus to power up
        vTaskDelay(pdMS_TO_TICKS(BUS_POWER_ON_DELAY));
        topology_sample();
        // start the timer! here while the bus is powered we will sample topology every minute
        configASSERT(xTimerStart(topology_timer,10));

        _sampling_enabled = true;

      } else if (_sampling_enabled && _bridge_power_controller->waitForSignal(false, portMAX_DELAY)) {
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
      while(!_bridge_power_controller->isPowerControlEnabled()) {
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
bool topology_sampler_get_node_list(uint64_t *node_list, size_t &node_list_size, uint32_t &num_nodes, uint32_t timeout_ms) {
  configASSERT(node_list);
  bool rval = false;
  if(xSemaphoreTake(_node_list.node_list_mutex, pdMS_TO_TICKS(timeout_ms))) {
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
