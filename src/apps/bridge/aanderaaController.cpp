#include "aanderaaController.h"
#include "aanderaa_data_msg.h"
#include "app_config.h"
#include "avgSampler.h"
#include "bm_network.h"
#include "bm_pubsub.h"
#include "bridgeLog.h"
#include "bridgePowerController.h"
#include "device_info.h"
#include "reportBuilder.h"
#include "semphr.h"
#include "sys_info_service.h"
#include "sys_info_svc_reply_msg.h"
#include "task_priorities.h"
#include "util.h"
#include <new>
#include "stm32_rtc.h"

// TODO - make this a configurable value?
#define MIN_READINGS_FOR_AGGREGATION 3
#define DEFAULT_CURRENT_READING_PERIOD_MS 60 * 1000 // default is 1 minute: 60,000 ms

TaskHandle_t aanderaa_controller_task_handle = NULL;

typedef struct Aanderaa {
  uint64_t node_id;
  Aanderaa *next;
  uint32_t current_agg_period_ms;
  AveragingSampler abs_speed_cm_s;
  AveragingSampler direction_rad;
  AveragingSampler temp_deg_c;

  static constexpr uint32_t N_SAMPLES_PAD =
      10; // Extra sample padding to account for timing slop.

  SemaphoreHandle_t _mutex;

public:
  bool subscribe();

private:
  static void aanderaSubCallback(uint64_t node_id, const char *topic, uint16_t topic_len,
                                 const uint8_t *data, uint16_t data_len, uint8_t type,
                                 uint8_t version);

private:
  static constexpr char subtag[] = "/aanderaa";
} Aanderaa_t;

typedef struct aanderaaControllerCtx {
  Aanderaa_t *_subbed_aanderaas;
  size_t _num_subbed_aanderaas;
  TaskHandle_t _task_handle;
  uint64_t _node_list[TOPOLOGY_SAMPLER_MAX_NODE_LIST_SIZE];
  bool _initialized;
  TimerHandle_t _sample_timer;
  BridgePowerController *_bridge_power_controller;
  cfg::Configuration *_sys_cfg;
  uint32_t current_reading_period_ms;
} aanderaaControllerCtx_t;

static aanderaaControllerCtx_t _ctx;

static constexpr uint32_t SAMPLE_TIMER_MS = 30 * 1000;
static constexpr uint32_t TOPO_TIMEOUT_MS = 10 * 1000;
static constexpr uint32_t NODE_INFO_TIMEOUT_MS = 1000;

#define PI  3.14159265358979323846

static constexpr double DIRECTION_SAMPLE_MEMBER_MIN = 0.0;
static constexpr double DIRECTION_SAMPLE_MEMBER_MAX = 2*PI;
static constexpr double ABS_SPEED_SAMPLE_MEMBER_MIN = 0.0;
static constexpr double ABS_SPEED_SAMPLE_MEMBER_MAX = 300.0;
static constexpr double TEMP_SAMPLE_MEMBER_MIN = -5.0;
static constexpr double TEMP_SAMPLE_MEMBER_MAX = 40.0;


static void createAanderaaSub(uint64_t node_id);
static void runController(void *param);
static Aanderaa_t *findAanderaaById(uint64_t node_id);
static void sampleTimerCallback(TimerHandle_t timer);
static bool node_info_reply_cb(bool ack, uint32_t msg_id, size_t service_strlen,
                               const char *service, size_t reply_len, uint8_t *reply_data);

bool Aanderaa::subscribe() {
  bool rval = false;
  char *sub = static_cast<char *>(pvPortMalloc(BM_TOPIC_MAX_LEN));
  configASSERT(sub);
  int topic_strlen = snprintf(sub, BM_TOPIC_MAX_LEN, "%" PRIx64 "%s", node_id, subtag);
  if (topic_strlen > 0) {
    rval = bm_sub_wl(sub, topic_strlen, aanderaSubCallback);
  }
  vPortFree(sub);
  return rval;
}

void Aanderaa::aanderaSubCallback(uint64_t node_id, const char *topic, uint16_t topic_len,
                                  const uint8_t *data, uint16_t data_len, uint8_t type,
                                  uint8_t version) {
  (void)type;
  (void)version;
  printf("Aanderaa data received from node %" PRIx64 " On topic: %.*s\n", node_id, topic_len,
         topic);
  Aanderaa_t *aanderaa = findAanderaaById(node_id);
  if (aanderaa) {
    if (xSemaphoreTake(aanderaa->_mutex, portMAX_DELAY)) {
      static AanderaaDataMsg::Data d;
      if (AanderaaDataMsg::decode(d, data, data_len) == CborNoError) {
        char *log_buf = static_cast<char *>(pvPortMalloc(SENSOR_LOG_BUF_SIZE));
        configASSERT(log_buf);
        aanderaa->abs_speed_cm_s.addSample(d.abs_speed_cm_s);
        aanderaa->direction_rad.addSample(degToRad(d.direction_deg_m));
        aanderaa->temp_deg_c.addSample(d.temperature_deg_c);
        size_t log_buflen = snprintf(
            log_buf, SENSOR_LOG_BUF_SIZE,
            "%" PRIx64 "," // Node Id
            "%" PRIu64 "," // reading_uptime_millis
            "%" PRIu64 "," // reading_time_utc_s
            "%" PRIu64 "," // sensor_reading_time_s
            "%.3f,"        // abs_speed_cm_s
            "%.3f,"        // abs_tilt_deg
            "%.3f,"        // direction_deg_m
            "%.3f,"        // east_cm_s
            "%.3f,"        // heading_deg_m
            "%.3f,"        // max_tilt_deg
            "%.3f,"        // ping_count
            "%.3f,"        // standard_ping_std_cm_s
            "%.3f,"        // std_tilt_deg
            "%.3f,"        // temperature_deg_c
            "%.3f\n",      // north_cm_s
            node_id, d.header.reading_uptime_millis, d.header.reading_time_utc_s,
            d.header.sensor_reading_time_s, d.abs_speed_cm_s, d.abs_tilt_deg, d.direction_deg_m,
            d.east_cm_s, d.heading_deg_m, d.max_tilt_deg, d.ping_count, d.standard_ping_std_cm_s,
            d.std_tilt_deg, d.temperature_deg_c, d.north_cm_s);
        if (log_buflen > 0) {
          BRIDGE_SENSOR_LOG_PRINTN(AANDERAA_IND, log_buf, log_buflen);
        } else {
          printf("ERROR: Failed to print Aanderaa data\n");
        }
        vPortFree(log_buf);
        xSemaphoreGive(aanderaa->_mutex);
      }
    } else {
      printf("Failed to get the subbed Aanderaa mutex after getting a new reading\n");
    }
  }
}

/*!
 * @brief Initialize the Aandera controller.
 * This controller is responsible for detecting Aanderaa nodes and subscribing to them.
 * It will also aggregate the data from the Aanderaa nodes and transmit it over the spotter_tx service.
 */
void aanderaControllerInit(BridgePowerController *power_controller,
                           cfg::Configuration *sys_cfg) {
  configASSERT(power_controller);
  configASSERT(sys_cfg);
  _ctx._bridge_power_controller = power_controller;
  _ctx._sys_cfg = sys_cfg;

  _ctx.current_reading_period_ms = DEFAULT_CURRENT_READING_PERIOD_MS;
  _ctx._sys_cfg->getConfig(AppConfig::CURRENT_READING_PERIOD_MS, strlen(AppConfig::CURRENT_READING_PERIOD_MS), _ctx.current_reading_period_ms);

  BaseType_t rval = xTaskCreate(runController, "Aandera Controller", 128 * 4, NULL,
                                AANDERAA_CONTROLLER_TASK_PRIORITY, &_ctx._task_handle);
  aanderaa_controller_task_handle = _ctx._task_handle;
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
  _ctx._subbed_aanderaas = NULL;
  _ctx._num_subbed_aanderaas = 0;
  _ctx._sample_timer = xTimerCreate("AanderaaCtlTim", pdMS_TO_TICKS(SAMPLE_TIMER_MS), pdTRUE,
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
        printf("Sampling for Aanderaa Nodes\n");
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
    if (task_notify_bits & AGGREGATION_TIMER_BITS) {
      printf("Aggregation period done!\n");
      if (_ctx._subbed_aanderaas != NULL) {
        Aanderaa_t *curr = _ctx._subbed_aanderaas;
        char *log_buf = static_cast<char *>(pvPortMalloc(SENSOR_LOG_BUF_SIZE));
        configASSERT(log_buf);
        while (curr != NULL) {
          if (xSemaphoreTake(curr->_mutex, portMAX_DELAY)) {
            size_t log_buflen = 0;
            aanderaa_aggregations_t agg = {.abs_speed_mean_cm_s = NAN,
                                           .abs_speed_std_cm_s = NAN,
                                           .direction_circ_mean_rad = NAN,
                                           .direction_circ_std_rad = NAN,
                                           .temp_mean_deg_c = NAN};
            // Check to make sure we have enough sensor readings for a valid aggregation.
            // If not send NaNs for all the values.
            // TODO - verify that we can assume if one sampler is below the min then all of them are.
            if(curr->abs_speed_cm_s.getNumSamples() >= MIN_READINGS_FOR_AGGREGATION) {
              agg.abs_speed_mean_cm_s = curr->abs_speed_cm_s.getMean(true);
              agg.abs_speed_std_cm_s = curr->abs_speed_cm_s.getStd(true);
              agg.direction_circ_mean_rad = curr->direction_rad.getCircularMean();
              agg.direction_circ_std_rad = curr->direction_rad.getCircularStd();
              agg.temp_mean_deg_c = curr->temp_deg_c.getMean(true);
              if (agg.abs_speed_mean_cm_s < ABS_SPEED_SAMPLE_MEMBER_MIN || agg.abs_speed_mean_cm_s > ABS_SPEED_SAMPLE_MEMBER_MAX) {
                agg.abs_speed_mean_cm_s = NAN;
              }
              if (agg.abs_speed_std_cm_s < ABS_SPEED_SAMPLE_MEMBER_MIN || agg.abs_speed_std_cm_s > ABS_SPEED_SAMPLE_MEMBER_MAX) {
              agg.abs_speed_std_cm_s = NAN;
              }

              // Translate the circular mean to be between 0 and 2π
              if (agg.direction_circ_mean_rad < DIRECTION_SAMPLE_MEMBER_MIN) {
                double translation = ceil(
                    (DIRECTION_SAMPLE_MEMBER_MIN - agg.direction_circ_mean_rad) / (2 * PI));
                agg.direction_circ_mean_rad += translation;
              } else if (agg.direction_circ_mean_rad > DIRECTION_SAMPLE_MEMBER_MAX) {
                double translation = floor(
                    (agg.direction_circ_mean_rad - DIRECTION_SAMPLE_MEMBER_MAX) / (2 * PI));
                agg.direction_circ_mean_rad -= translation;
              } else {
                agg.direction_circ_mean_rad = NAN;
              }

              // Translate the circular standard deviation to be between 0 and 2π
              if (agg.direction_circ_std_rad < DIRECTION_SAMPLE_MEMBER_MIN) {
                double translation =
                    ceil((DIRECTION_SAMPLE_MEMBER_MIN - agg.direction_circ_std_rad) / (2 * PI));
                agg.direction_circ_std_rad += translation;
              } else if (agg.direction_circ_std_rad > DIRECTION_SAMPLE_MEMBER_MAX) {
                double translation = floor(
                    (agg.direction_circ_std_rad - DIRECTION_SAMPLE_MEMBER_MAX) / (2 * PI));
                agg.direction_circ_std_rad -= translation;
              } else {
                agg.direction_circ_std_rad = NAN;
              }

              if (agg.temp_mean_deg_c < TEMP_SAMPLE_MEMBER_MIN || agg.temp_mean_deg_c > TEMP_SAMPLE_MEMBER_MAX) {
                agg.temp_mean_deg_c = NAN;
              }
            }
            static constexpr uint8_t TIME_STR_BUFSIZE = 50;
            static char timeStrbuf[TIME_STR_BUFSIZE];
            if(!logRtcGetTimeStr(timeStrbuf, TIME_STR_BUFSIZE,true)){
              printf("Failed to get time string for Aanderaa aggregation\n");
              snprintf(timeStrbuf, TIME_STR_BUFSIZE, "0");
            }; 
            log_buflen = snprintf(log_buf, SENSOR_LOG_BUF_SIZE,
                              "%s,"
                              "%" PRIx64 "," // Node Id
                              "%.3f,"        // abs_speed_mean_cm_s
                              "%.3f,"        // abs_speed_std_cm_s
                              "%.3f,"        // direction_circ_mean_rad
                              "%.3f,"        // direction_circ_std_rad
                              "%.3f\n",      // temp_mean_deg_c
                              timeStrbuf,
                              curr->node_id,
                              agg.abs_speed_mean_cm_s,
                              agg.abs_speed_std_cm_s,
                              agg.direction_circ_mean_rad,
                              agg.direction_circ_std_rad,
                              agg.temp_mean_deg_c);
            if (log_buflen > 0) {
              BRIDGE_SENSOR_LOG_PRINTN(AANDERAA_AGG, log_buf, log_buflen);
            } else {
              printf("ERROR: Failed to print Aanderaa data\n");
            }
            reportBuilderAddToQueue(curr->node_id, AANDERAA_SENSOR_TYPE, static_cast<void *>(&agg), sizeof(aanderaa_aggregations_t), REPORT_BUILDER_SAMPLE_MESSAGE);
            // TODO - send aggregated data to a "report builder" task that will
            // combine all the data from all the sensors and send it to the spotter
            memset(log_buf, 0, SENSOR_LOG_BUF_SIZE);
            // Clear the buffers
            curr->abs_speed_cm_s.clear();
            curr->direction_rad.clear();
            curr->temp_deg_c.clear();
            xSemaphoreGive(curr->_mutex);
          } else {
            printf("Failed to get the subbed Aanderaa mutex while trying to aggregate\n");
          }
          curr = curr->next;
        }
        vPortFree(log_buf);
        // The first four inputs are not used by this message type
        reportBuilderAddToQueue(0, 0, NULL, 0, REPORT_BUILDER_INCREMENT_SAMPLE_COUNT);
      } else {
        printf("No Aanderaa nodes to aggregate\n");
      }
    }
  }
}

static void createAanderaaSub(uint64_t node_id) {
  Aanderaa_t *new_sub = static_cast<Aanderaa_t *>(pvPortMalloc(sizeof(Aanderaa_t)));
  new_sub = new (new_sub) Aanderaa_t();
  configASSERT(new_sub);

  new_sub->_mutex = xSemaphoreCreateMutex();
  configASSERT(new_sub->_mutex);

  new_sub->node_id = node_id;
  new_sub->next = NULL;
  new_sub->current_agg_period_ms = (BridgePowerController::DEFAULT_SAMPLE_DURATION_S * 1000);
  if (_ctx._sys_cfg->getConfig(AppConfig::SAMPLE_DURATION_MS, strlen(AppConfig::SAMPLE_DURATION_MS),
                               new_sub->current_agg_period_ms)) {

  }
  uint32_t AVERAGER_MAX_SAMPLES =
      (new_sub->current_agg_period_ms / _ctx.current_reading_period_ms) + Aanderaa_t::N_SAMPLES_PAD;
  new_sub->abs_speed_cm_s.initBuffer(AVERAGER_MAX_SAMPLES);
  new_sub->direction_rad.initBuffer(AVERAGER_MAX_SAMPLES);
  new_sub->temp_deg_c.initBuffer(AVERAGER_MAX_SAMPLES);

  if (_ctx._subbed_aanderaas == NULL) {
    _ctx._subbed_aanderaas = new_sub;
  } else {
    Aanderaa_t *curr = _ctx._subbed_aanderaas;
    while (curr->next != NULL) {
      curr = curr->next;
    }
    curr->next = new_sub;
  }
  if (!new_sub->subscribe()) {
    printf("Failed to subscribe to Aanderaa node %" PRIx64 "\n", node_id);
  } else {
    printf("New Aanderaa node found %" PRIx64 "\n", node_id);
  }
}

static Aanderaa_t *findAanderaaById(uint64_t node_id) {
  Aanderaa_t *ret = NULL;
  Aanderaa_t *curr = _ctx._subbed_aanderaas;
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
        if (!findAanderaaById(reply.node_id)) {
          createAanderaaSub(reply.node_id);
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
