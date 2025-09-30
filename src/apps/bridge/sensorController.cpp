#include "sensorController.h"
#include "aanderaaConductivitySensor.h"
#include "aanderaaSensor.h"
#include "abstractSensor.h"
#include "app_config.h"
#include "app_util.h"
#include "bm_os.h"
#include "borealisSensor.h"
#include "bridgeLog.h"
#include "bridgePowerController.h"
#include "device_info.h"
#include "pmeDissolvedOxygenSensor.h"
#include "pmeWipeSensor.h"
#include "rbrCodaSensor.h"
#include "reportBuilder.h"
#include "seapointTurbiditySensor.h"
#include "softSensor.h"
#include "sys_info_service.h"
#include "sys_info_svc_reply_msg.h"
#include "task_priorities.h"

// TODO: Once we have bcmp_config request reply, we should read this value from the modules.
#define DEFAULT_CURRENT_READING_PERIOD_MS 60 * 1000       // default is 1 minute: 60,000 ms
#define DEFAULT_SOFT_READING_PERIOD_MS 500                // default is 500 ms (2 HZ)
#define DEFAULT_SEAPOINT_TURBIDITY_READING_PERIOD_MS 1000 // default is 1 second: 1000 ms (1 HZ)

TaskHandle_t sensor_controller_task_handle = NULL;

typedef struct sensorControllerCtx {
  AbstractSensor *_subbed_sensors;
  size_t _num_subbed_sensors;
  TaskHandle_t _task_handle;
  uint64_t _node_list[TOPOLOGY_SAMPLER_MAX_NODE_LIST_SIZE];
  bool _initialized;
  BridgePowerController *_bridge_power_controller;
  // TODO(bjh): Too tightly coupled to the sensor types. We can delegate a
  // function getReadingPeriodMs() to AbstractSensor and then call that function.
  uint32_t current_reading_period_ms;
  uint32_t soft_reading_period_ms;
  uint32_t rbr_coda_reading_period_ms;
  uint32_t seapoint_turbidity_reading_period_ms;
  uint32_t aanderaa_conductivity_reading_period_ms;
  uint32_t pme_dissolved_oxygen_reading_period_ms;
  uint32_t pme_wiper_reading_period_ms;
} sensorsControllerCtx_t;

typedef struct {
  uint32_t *ctx_field;
  uint32_t default_value;
  const char *config_key;
  const char *sensor_name;
} SensorConfigDef;

typedef struct {
  const char *app_name;
  abstractSensorType_e sensor_type;
  uint32_t *reading_period_ms;
  uint32_t samples_pad;
  AbstractSensor *(*create_fn)(uint64_t node_id, uint32_t sample_duration_ms,
                               uint32_t max_samples);
} SensorSubscriptionConfig;

static sensorsControllerCtx_t _ctx;

static constexpr uint32_t TOPO_TIMEOUT_MS = 10 * 1000;
static constexpr uint32_t NODE_INFO_TIMEOUT_MS = 1000;

static void runController(void *param);
static bool node_info_reply_cb(bool ack, uint32_t msg_id, size_t service_strlen,
                               const char *service, size_t reply_len, uint8_t *reply_data);
static void abstractSensorAddSensorSub(AbstractSensor *sensor);

/**
 * @brief Load a sensor's configuration from the config store.
 * @param config_def The definition of the sensor's configuration.
 * @param save Reference to a boolean that indicates if a save is needed.
 */
static void load_sensor_config(SensorConfigDef &config_def, bool &save) {
  if (!get_config_uint(BM_CFG_PARTITION_SYSTEM, config_def.config_key,
                       strlen(config_def.config_key), config_def.ctx_field)) {
    bridgeLogPrint(BRIDGE_CFG, BM_COMMON_LOG_LEVEL_INFO, USE_HEADER,
                   "Failed to get %s reading period from config, using default "
                   "value and writing to config: %" PRIu32 "ms\n",
                   config_def.sensor_name, *config_def.ctx_field);
    set_config_uint(BM_CFG_PARTITION_SYSTEM, config_def.config_key,
                    strlen(config_def.config_key), config_def.default_value);
    *config_def.ctx_field = config_def.default_value;
    save = true; // indicates save is needed
  }
}

/**
 * @brief Helper function to create and configure a sensor subscription
 * @param subscription_config The sensor subscription configuration
 * @param reply The decoded system info reply
 * @param sample_duration_ms Sample duration in milliseconds
 * @return true if sensor was processed (subscribed or already exists), false otherwise
 */
static bool
create_and_configure_sensor_subscription(const SensorSubscriptionConfig &subscription_config,
                                         const SysInfoReplyData &reply,
                                         uint32_t sample_duration_ms) {
  if (strncmp(reply.app_name, subscription_config.app_name,
              MIN(reply.app_name_strlen, strlen(subscription_config.app_name))) == 0) {
    if (!sensorControllerFindSensorById(reply.node_id, subscription_config.sensor_type) &&
        *subscription_config.reading_period_ms) {
      uint32_t AVERAGER_MAX_SAMPLES =
          (sample_duration_ms / *subscription_config.reading_period_ms) +
          subscription_config.samples_pad;
      AbstractSensor *sensor_sub = subscription_config.create_fn(
          reply.node_id, sample_duration_ms, AVERAGER_MAX_SAMPLES);
      if (sensor_sub) {
        abstractSensorAddSensorSub(sensor_sub);
      }
    }
    return true;
  }
  return false;
}

/*!
 * @brief Initialize the sensor controller.
 * This controller is responsible for identifying & detecting sensor nodes and subscribing to them.
 * It will also aggregate the data from the Aanderaa nodes and transmit it over the spotter_tx service.
 */
void sensorControllerInit(BridgePowerController *power_controller) {
  configASSERT(power_controller);
  _ctx._bridge_power_controller = power_controller;
  bool save = false;
  _ctx.current_reading_period_ms = DEFAULT_CURRENT_READING_PERIOD_MS;
  if (!get_config_uint(BM_CFG_PARTITION_SYSTEM, AppConfig::CURRENT_READING_PERIOD_MS,
                       strlen(AppConfig::CURRENT_READING_PERIOD_MS),
                       &_ctx.current_reading_period_ms)) {
    bridgeLogPrint(
        BRIDGE_CFG, BM_COMMON_LOG_LEVEL_INFO, USE_HEADER,
        "Failed to get current reading period from config, using default value and writing "
        "to config: %" PRIu32 "ms\n",
        _ctx.current_reading_period_ms);
    set_config_uint(BM_CFG_PARTITION_SYSTEM, AppConfig::CURRENT_READING_PERIOD_MS,
                    strlen(AppConfig::CURRENT_READING_PERIOD_MS),
                    _ctx.current_reading_period_ms);
    save = true;
  }

  _ctx.soft_reading_period_ms = DEFAULT_SOFT_READING_PERIOD_MS;
  if (!get_config_uint(BM_CFG_PARTITION_SYSTEM, AppConfig::SOFT_READING_PERIOD_MS,
                       strlen(AppConfig::SOFT_READING_PERIOD_MS),
                       &_ctx.soft_reading_period_ms)) {
    bridgeLogPrint(
        BRIDGE_CFG, BM_COMMON_LOG_LEVEL_INFO, USE_HEADER,
        "Failed to get soft reading period from config, using default value and writing "
        "to config: %" PRIu32 "ms\n",
        _ctx.soft_reading_period_ms);
    set_config_uint(BM_CFG_PARTITION_SYSTEM, AppConfig::SOFT_READING_PERIOD_MS,
                    strlen(AppConfig::SOFT_READING_PERIOD_MS), _ctx.soft_reading_period_ms);
    save = true;
  }

  _ctx.rbr_coda_reading_period_ms = RbrCodaSensor::DEFAULT_RBR_CODA_READING_PERIOD_MS;
  if (!get_config_uint(BM_CFG_PARTITION_SYSTEM, AppConfig::RBR_CODA_READING_PERIOD_MS,
                       strlen(AppConfig::RBR_CODA_READING_PERIOD_MS),
                       &_ctx.rbr_coda_reading_period_ms)) {
    bridgeLogPrint(
        BRIDGE_CFG, BM_COMMON_LOG_LEVEL_INFO, USE_HEADER,
        "Failed to get coda reading period from config, using default value and writing "
        "to config: %" PRIu32 "ms\n",
        _ctx.rbr_coda_reading_period_ms);
    set_config_uint(BM_CFG_PARTITION_SYSTEM, AppConfig::RBR_CODA_READING_PERIOD_MS,
                    strlen(AppConfig::RBR_CODA_READING_PERIOD_MS),
                    _ctx.rbr_coda_reading_period_ms);
    save = true;
  }

  _ctx.seapoint_turbidity_reading_period_ms = DEFAULT_SEAPOINT_TURBIDITY_READING_PERIOD_MS;
  if (!get_config_uint(BM_CFG_PARTITION_SYSTEM, AppConfig::TURBIDITY_READING_PERIOD_MS,
                       strlen(AppConfig::TURBIDITY_READING_PERIOD_MS),
                       &_ctx.seapoint_turbidity_reading_period_ms)) {
    bridgeLogPrint(BRIDGE_CFG, BM_COMMON_LOG_LEVEL_INFO, USE_HEADER,
                   "Failed to get seapoint_turbidity reading period from config, using default "
                   "value and writing "
                   "to config: %" PRIu32 "ms\n",
                   _ctx.seapoint_turbidity_reading_period_ms);
    set_config_uint(BM_CFG_PARTITION_SYSTEM, AppConfig::TURBIDITY_READING_PERIOD_MS,
                    strlen(AppConfig::TURBIDITY_READING_PERIOD_MS),
                    _ctx.seapoint_turbidity_reading_period_ms);
    save = true;
  }

  // If we like this pattern:
  // option 1: extend to other sensors, place structs in an array, iterate over array
  // option 2: delegate definition to sensor classes, like we did for AanderaaConductivitySensor::get_report_params
  SensorConfigDef conductivity_config = {
      .ctx_field = &_ctx.aanderaa_conductivity_reading_period_ms,
      .default_value = AanderaaConductivitySensor::get_default_reading_period_ms(),
      .config_key = AppConfig::AANDERAA_CONDUCTIVITY_READING_PERIOD_MS,
      .sensor_name = "aanderaa_conductivity"};
  load_sensor_config(conductivity_config, save);

  if (save) {
    save_config(BM_CFG_PARTITION_SYSTEM, false);
  }

  BaseType_t rval = xTaskCreate(runController, "Sensor Controller", 128 * 4, NULL,
                                SENSOR_CONTROLLER_TASK_PRIORITY, &_ctx._task_handle);
  sensor_controller_task_handle = _ctx._task_handle;
  configASSERT(rval == pdTRUE);
}

static void runController(void *param) {
  (void)param;
  uint32_t task_notify_bits;
  if (_ctx._initialized) {
    configASSERT(false); // Should only be initialized once
  }
  _ctx._subbed_sensors = NULL;
  _ctx._num_subbed_sensors = 0;
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
                printf("Failed to send sys_info request to node %016" PRIx64 "\n",
                       _ctx._node_list[i]);
              }
            }
          }
        }
      }
    }
    if (task_notify_bits & AGGREGATION_TIMER_BITS) {
      printf("Aggregation period done!\n");
      if (_ctx._subbed_sensors != NULL) {
        AbstractSensor *curr = _ctx._subbed_sensors;
        while (curr != NULL) {
          if (curr->type == SENSOR_TYPE_AANDERAA) {
            Aanderaa_t *aanderaa = static_cast<Aanderaa_t *>(curr);
            aanderaa->aggregate();
          } else if (curr->type == SENSOR_TYPE_SOFT) {
            Soft_t *soft = static_cast<Soft_t *>(curr);
            soft->aggregate();
          } else if (curr->type == SENSOR_TYPE_RBR_CODA) {
            RbrCoda_t *rbr_coda = static_cast<RbrCoda_t *>(curr);
            rbr_coda->aggregate();
          } else if (curr->type == SENSOR_TYPE_SEAPOINT_TURBIDITY) {
            SeapointTurbiditySensor *seapoint_turbidity =
                static_cast<SeapointTurbiditySensor *>(curr);
            seapoint_turbidity->aggregate();
          } else if (curr->type == SENSOR_TYPE_AANDERAA_CONDUCTIVITY) {
            // TODO(bjh): This cast _could be_ futile if AbstractSensor added
            //    aggregate as a virtual function. We should be able to collapse this loop to just:
            //    while { curr->aggregate(); cur = curr->next; }
            //    The only apparent value may be the check on curr->type, but that can be
            //    removed if we =
            //      a) trust the sensor creation or
            //      b) we can keep an array of valid sensor types, or
            //      c) we change the sensor types from #defines to enums.
            AanderaaConductivity_t *aanderaa_conductivity =
                static_cast<AanderaaConductivity_t *>(curr);
            aanderaa_conductivity->aggregate();
          } else if (curr->type == SENSOR_TYPE_PME_DO) {
            PmeDissolvedOxygenSensor *pme_dissolved_oxygen =
                static_cast<PmeDissolvedOxygenSensor *>(curr);
            pme_dissolved_oxygen->aggregate();
          } else if (curr->type == SENSOR_TYPE_BOREALIS) {
            BorealisSensor *level_statistics = static_cast<BorealisSensor *>(curr);
            level_statistics->aggregate();
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
    printf("Failed to subscribe to sensor node %016" PRIx64 "\n", sensor->node_id);
  } else {
    printf("New sensor node found %016" PRIx64 "\n", sensor->node_id);
  }
}

AbstractSensor *sensorControllerFindSensorById(uint64_t node_id, abstractSensorType_e type) {
  AbstractSensor *ret = NULL;
  AbstractSensor *curr = _ctx._subbed_sensors;
  while (curr != NULL) {
    if (curr->node_id == node_id && curr->type == type) {
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
  SysInfoReplyData reply = {0, 0, 0, 0, NULL};
  do {
    if (ack) {
      if (sys_info_reply_decode(&reply, reply_data, reply_len) != CborNoError) {
        printf("Failed to decode sys info reply\n");
        break;
      }

      // All sensors will have the same sample duration
      uint32_t sample_duration_ms = BridgePowerController::DEFAULT_SAMPLE_DURATION_S * 1000U;
      get_config_uint(BM_CFG_PARTITION_SYSTEM, AppConfig::SAMPLE_DURATION_MS,
                      strlen(AppConfig::SAMPLE_DURATION_MS), &sample_duration_ms);

      uint32_t subsample_duration_ms =
          BridgePowerController::DEFAULT_SUBSAMPLE_DURATION_S * 1000U;
      get_config_uint(BM_CFG_PARTITION_SYSTEM, AppConfig::SUBSAMPLE_DURATION_MS,
                      strlen(AppConfig::SUBSAMPLE_DURATION_MS), &subsample_duration_ms);

      uint32_t subsample_intertval_ms =
          BridgePowerController::DEFAULT_SUBSAMPLE_INTERVAL_S * 1000U;
      get_config_uint(BM_CFG_PARTITION_SYSTEM, AppConfig::SUBSAMPLE_INTERVAL_MS,
                      strlen(AppConfig::SUBSAMPLE_INTERVAL_MS), &subsample_intertval_ms);

      uint32_t subsample_enabled = BridgePowerController::DEFAULT_SUBSAMPLE_ENABLED;
      get_config_uint(BM_CFG_PARTITION_SYSTEM, AppConfig::SUBSAMPLE_ENABLED,
                      strlen(AppConfig::SUBSAMPLE_ENABLED), &subsample_enabled);

      // Define Aanderaa conductivity subscription config
      static const SensorSubscriptionConfig aanderaa_conductivity_config = {
          .app_name = "aanderaa_conductivity",
          .sensor_type = SENSOR_TYPE_AANDERAA_CONDUCTIVITY,
          .reading_period_ms = &_ctx.aanderaa_conductivity_reading_period_ms,
          .samples_pad = AanderaaConductivity_t::N_SAMPLES_PAD,
          .create_fn = [](uint64_t node_id, uint32_t duration,
                          uint32_t samples) -> AbstractSensor * {
            return createAanderaaConductivitySub(node_id, duration, samples);
          }
          // .create_fn = &createAanderaaConductivitySub//(node_id, duration, samples)
      };

      // TODO(bjh): Observation: it would be more efficient to do a hash table lookup of app_name
      //    and then map to the SensorSubscriptionConfig.
      if (create_and_configure_sensor_subscription(aanderaa_conductivity_config, reply,
                                                   sample_duration_ms)) {
        // Aanderaa conductivity handled by helper function
      } else if (strncmp(reply.app_name, "aanderaa",
                         MIN(reply.app_name_strlen, strlen("aanderaa"))) == 0) {
        if (!sensorControllerFindSensorById(reply.node_id, SENSOR_TYPE_AANDERAA)) {
          uint32_t AVERAGER_MAX_SAMPLES =
              (sample_duration_ms / _ctx.current_reading_period_ms) + Aanderaa_t::N_SAMPLES_PAD;
          Aanderaa_t *aanderaa_sub =
              createAanderaaSub(reply.node_id, sample_duration_ms, AVERAGER_MAX_SAMPLES);
          if (aanderaa_sub) {
            abstractSensorAddSensorSub(aanderaa_sub);
          }
        }
      } else if (strncmp(reply.app_name, "bm_soft_module",
                         MIN(reply.app_name_strlen, strlen("bm_soft_module"))) == 0) {
        if (!sensorControllerFindSensorById(reply.node_id, SENSOR_TYPE_SOFT)) {
          uint32_t AVERAGER_MAX_SAMPLES =
              (sample_duration_ms / _ctx.soft_reading_period_ms) + Soft_t::N_SAMPLES_PAD;
          Soft_t *soft_sub =
              createSoftSub(reply.node_id, sample_duration_ms, AVERAGER_MAX_SAMPLES);
          if (soft_sub) {
            abstractSensorAddSensorSub(soft_sub);
          }
        }
      } else if (strncmp(reply.app_name, "bm_rbr",
                         MIN(reply.app_name_strlen, strlen("bm_rbr"))) == 0) {
        if (!sensorControllerFindSensorById(reply.node_id, SENSOR_TYPE_RBR_CODA)) {
          uint32_t AVERAGER_MAX_SAMPLES =
              (sample_duration_ms / _ctx.rbr_coda_reading_period_ms) + RbrCoda_t::N_SAMPLES_PAD;
          RbrCoda_t *rbr_coda_sub =
              createRbrCodaSub(reply.node_id, sample_duration_ms, AVERAGER_MAX_SAMPLES,
                               _ctx.rbr_coda_reading_period_ms);
          if (rbr_coda_sub) {
            abstractSensorAddSensorSub(rbr_coda_sub);
          }
        }
      } else if (strncmp(reply.app_name, "seapoint_turbidity",
                         MIN(reply.app_name_strlen, strlen("seapoint_turbidity"))) == 0) {
        if (!sensorControllerFindSensorById(reply.node_id, SENSOR_TYPE_SEAPOINT_TURBIDITY)) {
          uint32_t AVERAGER_MAX_SAMPLES =
              (sample_duration_ms / _ctx.seapoint_turbidity_reading_period_ms) +
              SeapointTurbidity_t::N_SAMPLES_PAD;
          SeapointTurbidity_t *seapoint_turbidity_sub = createSeapointTurbiditySub(
              reply.node_id, sample_duration_ms, AVERAGER_MAX_SAMPLES);
          if (seapoint_turbidity_sub) {
            abstractSensorAddSensorSub(seapoint_turbidity_sub);
          }
        }
      } else if (strncmp(reply.app_name, "pme_do_sensor",
                         MIN(reply.app_name_strlen, strlen("pme_do_sensor"))) == 0) {
        if (!sensorControllerFindSensorById(reply.node_id, SENSOR_TYPE_PME_DO)) {
          PmeDissolvedOxygen_t *pme_dissolved_oxygen_sub = createPmeDissolvedOxygenSub(
              reply.node_id, sample_duration_ms, subsample_intertval_ms, subsample_duration_ms,
              static_cast<bool>(subsample_enabled));
          if (pme_dissolved_oxygen_sub) {
            abstractSensorAddSensorSub(pme_dissolved_oxygen_sub);
          }
          PmeWipe_t *pme_wiper_sub = createPmeWipeSub(reply.node_id);
          if (pme_wiper_sub) {
            abstractSensorAddSensorSub(pme_wiper_sub);
          }
        }
      } else if (strncmp(reply.app_name, "borealis",
                         MIN(reply.app_name_strlen, strlen("borealis"))) == 0) {
        if (!sensorControllerFindSensorById(reply.node_id, SENSOR_TYPE_BOREALIS)) {
          Borealis_t *borealis_sub = createBorealisSensorSub(reply.node_id);

          if (borealis_sub) {
            abstractSensorAddSensorSub(borealis_sub);
          } else {
            bm_free(borealis_sub);
          }
        }
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
