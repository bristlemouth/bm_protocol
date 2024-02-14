#include "sensorController.h"
#include "aanderaaSensor.h"
#include "app_config.h"
#include "bridgePowerController.h"
#include "device_info.h"
#include "rbrCodaSensor.h"
#include "reportBuilder.h"
#include "softSensor.h"
#include "sys_info_service.h"
#include "sys_info_svc_reply_msg.h"
#include "task_priorities.h"
#include "util.h"

// TODO: Once we have bcmp_config request reply, we should read this value from the modules.
#define DEFAULT_CURRENT_READING_PERIOD_MS 60 * 1000 // default is 1 minute: 60,000 ms
#define DEFAULT_SOFT_READING_PERIOD_MS 500 // default is 500 ms (2 HZ)
#define DEFAULT_RBR_CODA_READING_PERIOD_MS 500 // default is 500 ms (2 HZ)

TaskHandle_t sensor_controller_task_handle = NULL;

typedef struct sensorControllerCtx {
  AbstractSensor *_subbed_sensors;
  size_t _num_subbed_sensors;
  TaskHandle_t _task_handle;
  uint64_t _node_list[TOPOLOGY_SAMPLER_MAX_NODE_LIST_SIZE];
  bool _initialized;
  TimerHandle_t _sample_timer;
  BridgePowerController *_bridge_power_controller;
  cfg::Configuration *_sys_cfg;
  uint32_t current_reading_period_ms;
  uint32_t soft_reading_period_ms;
  uint32_t rbr_coda_agg_period_ms;
} sensorsControllerCtx_t;

static sensorsControllerCtx_t _ctx;

static constexpr uint32_t SAMPLE_TIMER_MS = 30 * 1000;
static constexpr uint32_t TOPO_TIMEOUT_MS = 10 * 1000;
static constexpr uint32_t NODE_INFO_TIMEOUT_MS = 1000;

static void runController(void *param);
static void sampleTimerCallback(TimerHandle_t timer);
static bool node_info_reply_cb(bool ack, uint32_t msg_id, size_t service_strlen,
                               const char *service, size_t reply_len, uint8_t *reply_data);
static void abstractSensorAddSensorSub(AbstractSensor *sensor);
/*!
 * @brief Initialize the sensor controller.
 * This controller is responsible for identifying & detecting sensor nodes and subscribing to them.
 * It will also aggregate the data from the Aanderaa nodes and transmit it over the spotter_tx service.
 */
void sensorControllerInit(BridgePowerController *power_controller,
                           cfg::Configuration *sys_cfg) {
  configASSERT(power_controller);
  configASSERT(sys_cfg);
  _ctx._bridge_power_controller = power_controller;
  _ctx._sys_cfg = sys_cfg;

  _ctx.current_reading_period_ms = DEFAULT_CURRENT_READING_PERIOD_MS;
  _ctx._sys_cfg->getConfig(AppConfig::CURRENT_READING_PERIOD_MS, strlen(AppConfig::CURRENT_READING_PERIOD_MS), _ctx.current_reading_period_ms);

  _ctx.soft_reading_period_ms = DEFAULT_SOFT_READING_PERIOD_MS;
  _ctx._sys_cfg->getConfig(AppConfig::SOFT_READING_PERIOD_MS, strlen(AppConfig::SOFT_READING_PERIOD_MS), _ctx.soft_reading_period_ms);

  _ctx.rbr_coda_agg_period_ms = DEFAULT_RBR_CODA_READING_PERIOD_MS;
  _ctx._sys_cfg->getConfig(AppConfig::RBR_CODA_READING_PERIOD_MS, strlen(AppConfig::RBR_CODA_READING_PERIOD_MS), _ctx.rbr_coda_agg_period_ms);

  BaseType_t rval = xTaskCreate(runController, "Sensor Controller", 128 * 4, NULL,
                                SENSOR_CONTROLLER_TASK_PRIORITY, &_ctx._task_handle);
  sensor_controller_task_handle = _ctx._task_handle;
  configASSERT(rval == pdTRUE);
}

static void sampleTimerCallback(TimerHandle_t timer) {
  (void)timer;
  xTaskNotify(_ctx._task_handle, SAMPLER_TIMER_BITS, eSetBits);
}

static void runController(void *param) {
  (void)param;
  uint32_t task_notify_bits;
  if (_ctx._initialized) {
    configASSERT(false); // Should only be initialized once
  }
  _ctx._subbed_sensors = NULL;
  _ctx._num_subbed_sensors = 0;
  _ctx._sample_timer = xTimerCreate("SensorCtlTim", pdMS_TO_TICKS(SAMPLE_TIMER_MS), pdTRUE,
                                    NULL, sampleTimerCallback);
  configASSERT(_ctx._sample_timer);
  configASSERT(xTimerStart(_ctx._sample_timer, 10) == pdTRUE);
  _ctx._initialized = true;
  while (true) {
    // wait for a notification from one of the timers, clear all the bits on exit
    xTaskNotifyWait(pdFALSE, UINT32_MAX, &task_notify_bits, portMAX_DELAY);
    if (task_notify_bits & SAMPLER_TIMER_BITS) {
      if (_ctx._bridge_power_controller->waitForSignal(true, pdMS_TO_TICKS(TOPO_TIMEOUT_MS))) {
        size_t size_list = sizeof(_ctx._node_list);
        printf("Sampling for sensor nodes\n");
        uint32_t num_nodes = 0;
        if (topology_sampler_get_node_list(_ctx._node_list, size_list, num_nodes,
                                          TOPO_TIMEOUT_MS)) {
          for (size_t i = 0; i < num_nodes; i++) {
            if (_ctx._node_list[i] != getNodeId()) {
              if (!sys_info_service_request(_ctx._node_list[i], node_info_reply_cb,
                                            NODE_INFO_TIMEOUT_MS)) {
                printf("Failed to send sys_info request to node %" PRIx64 "\n",
                      _ctx._node_list[i]);
              }
            }
          }
        }
      }
    }
    if (task_notify_bits & AANDERAA_AGGREGATION_TIMER_BITS) {
      printf("Aggregation period done!\n");
      if (_ctx._subbed_sensors != NULL) {
        AbstractSensor *curr = _ctx._subbed_sensors;
        while (curr != NULL) {
          if(curr->type == SENSOR_TYPE_AANDERAA){
            Aanderaa_t* aanderaa = static_cast<Aanderaa_t*>(curr);
            aanderaa->aggregate();
          } else if (curr->type == SENSOR_TYPE_SOFT) {
            Soft_t *soft = static_cast<Soft_t *>(curr);
            soft->aggregate();
          } else if (curr->type == SENSOR_TYPE_RBR_CODA) {
            RbrCoda_t *rbr_coda = static_cast<RbrCoda_t *>(curr);
            rbr_coda->aggregate();
          }
          curr = curr->next;
        }
        // The first four inputs are not used by this message type
        reportBuilderAddToQueue(0, 0, NULL, 0, REPORT_BUILDER_INCREMENT_SAMPLE_COUNT);
      } else {
        printf("No sensor nodes to aggregate\n");
      }
    }
  }
}

void abstractSensorAddSensorSub(AbstractSensor *sensor) {
  if (_ctx._subbed_sensors == NULL) {
    _ctx._subbed_sensors = sensor;
  } else {
    AbstractSensor *curr = _ctx._subbed_sensors;
    while (curr->next != NULL) {
      curr = curr->next;
    }
    curr->next = sensor;
  }
  if (!sensor->subscribe()) {
    printf("Failed to subscribe to sensor node %" PRIx64 "\n", sensor->node_id);
  } else {
    printf("New sensor node found %" PRIx64 "\n", sensor->node_id);
  }
}

AbstractSensor *sensorControllerFindSensorById(uint64_t node_id) {
  AbstractSensor *ret = NULL;
  AbstractSensor *curr = _ctx._subbed_sensors;
  while (curr != NULL) {
    if (curr->node_id == node_id) {
      ret = curr;
      break;
    }
    curr = curr->next;
  }
  return ret;
}

static bool node_info_reply_cb(bool ack, uint32_t msg_id, size_t service_strlen,
                               const char *service, size_t reply_len, uint8_t *reply_data) {
  (void)service_strlen;
  (void)service;
  bool rval = false;
  printf("Msg id: %" PRIu32 "\n", msg_id);
  SysInfoSvcReplyMsg::Data reply = {0, 0, 0, 0, NULL};
  do {
    if (ack) {
      if (SysInfoSvcReplyMsg::decode(reply, reply_data, reply_len) != CborNoError) {
        printf("Failed to decode sys info reply\n");
        break;
      }
      if (strncmp(reply.app_name, "aanderaa", MIN(reply.app_name_strlen, strlen("aanderaa"))) ==
          0) {
        if (!sensorControllerFindSensorById(reply.node_id)) {
            uint32_t current_agg_period_ms = (BridgePowerController::DEFAULT_SAMPLE_DURATION_S * 1000);
            _ctx._sys_cfg->getConfig(AppConfig::SAMPLE_DURATION_MS, strlen(AppConfig::SAMPLE_DURATION_MS),
                                        current_agg_period_ms);
            uint32_t AVERAGER_MAX_SAMPLES =
                (current_agg_period_ms / _ctx.current_reading_period_ms) + Aanderaa_t::N_SAMPLES_PAD;
            Aanderaa_t * aanderaa_sub = createAanderaaSub(reply.node_id, current_agg_period_ms, AVERAGER_MAX_SAMPLES);
            if(aanderaa_sub){
                abstractSensorAddSensorSub(aanderaa_sub);
            }
        }
      } else if (strncmp(reply.app_name, "bm_soft_module", MIN(reply.app_name_strlen, strlen("bm_soft_module"))) == 0) {
        if (!sensorControllerFindSensorById(reply.node_id)) {
          uint32_t soft_agg_period_ms = (BridgePowerController::DEFAULT_SAMPLE_DURATION_S * 1000);
          _ctx._sys_cfg->getConfig(AppConfig::SAMPLE_DURATION_MS, strlen(AppConfig::SAMPLE_DURATION_MS),
                                        soft_agg_period_ms);
            uint32_t AVERAGER_MAX_SAMPLES =
                (soft_agg_period_ms / _ctx.soft_reading_period_ms) + Soft_t::N_SAMPLES_PAD;
            Soft_t * soft_sub = createSoftSub(reply.node_id, soft_agg_period_ms, AVERAGER_MAX_SAMPLES);
            if(soft_sub){
                abstractSensorAddSensorSub(soft_sub);
            }
        }
        // TODO - remove this print statement
        printf("Soft sensor node found %" PRIx64 "\n", reply.node_id);
      } else if (strncmp(reply.app_name, "bm_rbr", MIN(reply.app_name_strlen, strlen("bm_rbr"))) == 0) {
        if (!sensorControllerFindSensorById(reply.node_id)) {
          uint32_t rbr_coda_agg_period_ms = (BridgePowerController::DEFAULT_SAMPLE_DURATION_S * 1000);
          _ctx._sys_cfg->getConfig(AppConfig::SAMPLE_DURATION_MS, strlen(AppConfig::SAMPLE_DURATION_MS),
                                        rbr_coda_agg_period_ms);
          uint32_t AVERAGER_MAX_SAMPLES =
              (rbr_coda_agg_period_ms / _ctx.rbr_coda_agg_period_ms) + RbrCoda_t::N_SAMPLES_PAD;
          RbrCoda_t * rbr_coda_sub = createRbrCodaSub(reply.node_id, rbr_coda_agg_period_ms, AVERAGER_MAX_SAMPLES);
          if(rbr_coda_sub){
              abstractSensorAddSensorSub(rbr_coda_sub);
          }
        }
        // TODO - remove this print statement after done testing
        printf("RBR CODA sensor node found %" PRIx64 "\n", reply.node_id);
      }
    } else {
      printf("NACK\n");
    }
    rval = true;
  } while (0);
  if (reply.app_name) {
    vPortFree(reply.app_name);
  }
  return rval;
}
