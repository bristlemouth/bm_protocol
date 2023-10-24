#include "aanderaaController.h"
#include "aanderaa_data_msg.h"
#include "avgSampler.h"
#include "bm_network.h"
#include "bm_pubsub.h"
#include "bridgeLog.h"
#include "device_info.h"
#include "sys_info_service.h"
#include "sys_info_svc_reply_msg.h"
#include "task_priorities.h"
#include "util.h"
#include <new>

typedef struct Aanderaa {
  uint64_t node_id;
  Aanderaa *next;
  AveragingSampler abs_speed_mean_cm_s;
  AveragingSampler abs_speed_std_cm_s;
  AveragingSampler direction_circ_mean_rad;
  AveragingSampler direction_circ_std_rad;
  AveragingSampler temp_mean_deg_c;

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
} aanderaaControllerCtx_t;

static aanderaaControllerCtx_t _ctx;

static constexpr uint32_t SAMPLE_TIMER_MS = 30 * 1000;
static constexpr uint32_t TOPO_TIMEOUT_MS = 10 * 1000;
static constexpr uint32_t NODE_INFO_TIMEOUT_MS = 1000;
static constexpr uint8_t AANDERAA_AVERAGER_MAX_SAMPLES = 10;

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
  (void)data;
  (void)data_len;
  (void)type;
  (void)version;
  printf("Aanderaa data received from node %" PRIx64 " On topic: %.*s\n", node_id, topic_len,
         topic);
  Aanderaa_t *aanderaa = findAanderaaById(node_id);
  if (aanderaa) {
    static AanderaaDataMsg::Data d;
    if (AanderaaDataMsg::decode(d, data, data_len) == CborNoError) {
      char *log_buf = static_cast<char *>(pvPortMalloc(SENSOR_LOG_BUF_SIZE));
      configASSERT(log_buf);
      aanderaa->abs_speed_mean_cm_s.addSample(d.abs_speed_cm_s);
      aanderaa->abs_speed_std_cm_s.addSample(d.abs_speed_cm_s);
      aanderaa->direction_circ_mean_rad.addSample(degToRad(d.direction_deg_m));
      aanderaa->direction_circ_std_rad.addSample(degToRad(d.direction_deg_m));
      aanderaa->temp_mean_deg_c.addSample(d.temperature_deg_c);
      size_t log_buflen =
          snprintf(log_buf, SENSOR_LOG_BUF_SIZE,
                   "%" PRIx64 "," // Node Id
                   "%" PRIu64 "," // Uptime
                   "%.3f," // abs_speed_cm_s
                   "%.3f," // abs_tilt_deg
                   "%.3f," // direction_deg_m
                   "%.3f," // east_cm_s
                   "%.3f," // heading_deg_m
                   "%.3f," // max_tilt_deg
                   "%.3f," // ping_count
                   "%.3f," // standard_ping_std_cm_s
                   "%.3f," // std_tilt_deg
                   "%.3f," // temperature_deg_c
                   "%.3f\n", // north_cm_s
                   node_id, d.header.reading_uptime_millis, d.abs_speed_cm_s, d.abs_tilt_deg, d.direction_deg_m, d.east_cm_s,
                   d.heading_deg_m, d.max_tilt_deg, d.ping_count, d.standard_ping_std_cm_s,
                   d.std_tilt_deg, d.temperature_deg_c, d.north_cm_s);
      if (log_buflen > 0) {
        BRIDGE_SENSOR_LOG_PRINTN(AANDERAA_IND, log_buf, log_buflen);
      } else {
        printf("ERROR: Failed to print Aanderaa data\n");
      }
      if (aanderaa->abs_speed_mean_cm_s.getNumSamples() == AANDERAA_AVERAGER_MAX_SAMPLES) {
        log_buflen = snprintf(log_buf, SENSOR_LOG_BUF_SIZE,
                              "%" PRIx64 "," // Node Id
                              "%.3f," // abs_speed_mean_cm_s
                              "%.3f," // abs_speed_std_cm_s
                              "%.3f," // direction_circ_mean_rad
                              "%.3f," // direction_circ_std_rad
                              "%.3f\n", // temp_mean_deg_c
                              node_id,
                              aanderaa->abs_speed_mean_cm_s.getMean(),
                              aanderaa->abs_speed_std_cm_s.getStd(),
                              aanderaa->direction_circ_mean_rad.getCircularMean(),
                              aanderaa->direction_circ_std_rad.getCircularStd(),
                              aanderaa->temp_mean_deg_c.getMean());
        if (log_buflen > 0) {
          BRIDGE_SENSOR_LOG_PRINTN(AANDERAA_AGG, log_buf, log_buflen);
        } else {
          printf("ERROR: Failed to print Aanderaa data\n");
        }
        // TODO transmit data to spotter once we define it.
        aanderaa->abs_speed_mean_cm_s.clear();
        aanderaa->abs_speed_std_cm_s.clear();
        aanderaa->direction_circ_mean_rad.clear();
        aanderaa->direction_circ_std_rad.clear();
        aanderaa->temp_mean_deg_c.clear();
      }
      vPortFree(log_buf);
    }
  }
}

/*!
 * @brief Initialize the Aandera controller.
 * This controller is responsible for detecting Aanderaa nodes and subscribing to them.
 * It will also aggregate the data from the Aanderaa nodes and transmit it over the spotter_tx service.
 */
void aanderaControllerInit(BridgePowerController *power_controller) {
  configASSERT(power_controller);
  _ctx._bridge_power_controller = power_controller;
  BaseType_t rval = xTaskCreate(runController, "Aandera Controller", 128 * 4, NULL,
                                AANDERAA_CONTROLLER_TASK_PRIORITY, &_ctx._task_handle);
  configASSERT(rval == pdTRUE);
}

static void sampleTimerCallback(TimerHandle_t timer) {
  (void)timer;
  xTaskNotifyGive(_ctx._task_handle);
}

static void runController(void *param) {
  (void)param;
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
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
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
}

static void createAanderaaSub(uint64_t node_id) {
  Aanderaa_t *new_sub = static_cast<Aanderaa_t *>(pvPortMalloc(sizeof(Aanderaa_t)));
  new_sub = new (new_sub) Aanderaa_t();
  configASSERT(new_sub);
  new_sub->node_id = node_id;
  new_sub->next = NULL;
  new_sub->abs_speed_mean_cm_s.initBuffer(AANDERAA_AVERAGER_MAX_SAMPLES);
  new_sub->abs_speed_std_cm_s.initBuffer(AANDERAA_AVERAGER_MAX_SAMPLES);
  new_sub->direction_circ_mean_rad.initBuffer(AANDERAA_AVERAGER_MAX_SAMPLES);
  new_sub->direction_circ_std_rad.initBuffer(AANDERAA_AVERAGER_MAX_SAMPLES);
  new_sub->temp_mean_deg_c.initBuffer(AANDERAA_AVERAGER_MAX_SAMPLES);
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
