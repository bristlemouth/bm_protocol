#include "seapointTurbiditySensor.h"
#include "app_config.h"
#include "avgSampler.h"
#include "spotter.h"
#include "pubsub.h"
#include "bm_seapoint_turbidity_data_msg.h"
#include "bridgeLog.h"
#include "cbor.h"
#include "device_info.h"
#include "reportBuilder.h"
#include "semphr.h"
#include "stm32_rtc.h"
#include "topology_sampler.h"
#include "app_util.h"
#include <new>

#define DEFAULT_TURBIDITY_READING_PERIOD_MS 1000 // 1 second

bool SeapointTurbiditySensor::subscribe() {
  bool rval = false;
  char *sub = static_cast<char *>(pvPortMalloc(BM_TOPIC_MAX_LEN));
  configASSERT(sub);
  int topic_strlen =
      snprintf(sub, BM_TOPIC_MAX_LEN, "sensor/%016" PRIx64 "%s", node_id, subtag);
  if (topic_strlen > 0) {
    rval = bm_sub_wl(sub, topic_strlen, seapointTurbiditySubCallback) == BmOK;
  }
  vPortFree(sub);
  return rval;
}

void SeapointTurbiditySensor::seapointTurbiditySubCallback(uint64_t node_id, const char *topic,
                                                           uint16_t topic_len,
                                                           const uint8_t *data,
                                                           uint16_t data_len, uint8_t type,
                                                           uint8_t version) {
  (void)type;
  (void)version;
  printf("Seapoint Turbidity data received from node %016" PRIx64 ", on topic: %.*s\n", node_id,
         topic_len, topic);
  SeapointTurbidity_t *turbidity_sensor =
      static_cast<SeapointTurbidity_t *>(sensorControllerFindSensorById(node_id));
  if (turbidity_sensor && turbidity_sensor->type == SENSOR_TYPE_SEAPOINT_TURBIDITY) {
    if (xSemaphoreTake(turbidity_sensor->_mutex, portMAX_DELAY)) {
      static BmSeapointTurbidityDataMsg::Data turbidity_data;
      if (BmSeapointTurbidityDataMsg::decode(turbidity_data, data, data_len) == CborNoError) {
        char *log_buf = static_cast<char *>(pvPortMalloc(SENSOR_LOG_BUF_SIZE));
        configASSERT(log_buf);
        turbidity_sensor->turbidity_s_ftu.addSample(turbidity_data.s_signal);
        turbidity_sensor->turbidity_r_ftu.addSample(turbidity_data.r_signal);
        turbidity_sensor->reading_count++;
        // Large floats get formatted in scientific notation,
        // so we print integer seconds and millis separately.
        uint64_t reading_time_sec = turbidity_data.header.reading_time_utc_ms / 1000U;
        uint32_t reading_time_millis = turbidity_data.header.reading_time_utc_ms % 1000U;
        uint64_t sensor_reading_time_sec = turbidity_data.header.sensor_reading_time_ms / 1000U;
        uint32_t sensor_reading_time_millis =
            turbidity_data.header.sensor_reading_time_ms % 1000U;

        uint32_t current_timestamp = pdTICKS_TO_MS(xTaskGetTickCount());
        if (current_timestamp - turbidity_sensor->last_timestamp >
                DEFAULT_TURBIDITY_READING_PERIOD_MS + 1000U ||
            turbidity_sensor->reading_count == 1U) {
          printf("Updating Seapoint Turbidity %016" PRIx64
                 " node position, current_time = %" PRIu32 ", last_time = %" PRIu32
                 ", reading count: %" PRIu32 "\n",
                 node_id, current_timestamp, turbidity_sensor->last_timestamp,
                 turbidity_sensor->reading_count);
          turbidity_sensor->node_position =
              topology_sampler_get_node_position(node_id, pdTICKS_TO_MS(5000));
        }
        turbidity_sensor->last_timestamp = current_timestamp;

        size_t log_buflen =
            snprintf(log_buf, SENSOR_LOG_BUF_SIZE,
                     "%016" PRIx64 ","     // Node Id
                     "%" PRIi8 ","         // node_position
                     "seapoint_turbidity," // node_app_name
                     "%" PRIu64 ","        // reading_uptime_millis
                     "%" PRIu64 "."        // reading_time_utc_ms seconds part
                     "%03" PRIu32 ","      // reading_time_utc_ms millis part
                     "%" PRIu64 "."        // sensor_reading_time_ms seconds part
                     "%03" PRIu32 ","      // sensor_reading_time_ms millis part
                     "%.4f,"               // s_signal
                     "%.3f\n",             // r_signal
                     node_id, turbidity_sensor->node_position,
                     turbidity_data.header.reading_uptime_millis, reading_time_sec,
                     reading_time_millis, sensor_reading_time_sec, sensor_reading_time_millis,
                     turbidity_data.s_signal, turbidity_data.r_signal);
        if (log_buflen > 0) {
          BRIDGE_SENSOR_LOG_PRINTN(BM_COMMON_IND, log_buf, log_buflen);
        } else {
          printf("ERROR: Failed to print Seapoint Turbidity individual reading to log\n");
        }
        vPortFree(log_buf);
      }
      xSemaphoreGive(turbidity_sensor->_mutex);
    } else {
      printf(
          "Failed to take mutex for Seapoint Turbidity Sensor after getting a new reading\n");
    }
  }
}

void SeapointTurbiditySensor::aggregate(void) {
  char *log_buf = static_cast<char *>(pvPortMalloc(SENSOR_LOG_BUF_SIZE));
  configASSERT(log_buf);
  if (xSemaphoreTake(_mutex, portMAX_DELAY)) {
    size_t log_buflen = 0;
    seapoint_turbidity_aggregations_t turbidity_aggs = {
        .turbidity_s_mean_ftu = NAN, .turbidity_r_mean_ftu = NAN, .reading_count = 0};
    if (turbidity_s_ftu.getNumSamples() > MIN_READINGS_FOR_AGGREGATION) {
      turbidity_aggs.turbidity_s_mean_ftu = turbidity_s_ftu.getMean();
      turbidity_aggs.turbidity_r_mean_ftu = turbidity_r_ftu.getMean();
      turbidity_aggs.reading_count = reading_count;
    }
    static constexpr uint8_t TIME_STR_BUFSIZE = 50;
    char time_str[TIME_STR_BUFSIZE];
    if (!logRtcGetTimeStr(time_str, TIME_STR_BUFSIZE, true)) {
      printf("Failed to get RTC time string for Seapoint Turbidity aggregation\n");
      snprintf(time_str, TIME_STR_BUFSIZE, "0");
    }

    int8_t node_position = topology_sampler_get_node_position(node_id, pdTICKS_TO_MS(5000));

    log_buflen =
        snprintf(log_buf, SENSOR_LOG_BUF_SIZE,
                 "%016" PRIx64 ","     // Node Id
                 "%" PRIi8 ","         // node_position
                 "seapoint_turbidity," // node_app_name
                 "%s,"                 // timestamp(ticks/UTC)
                 "%" PRIu32 ","        // reading_count
                 "%.4f,"               // turbidity_s_mean_ftu
                 "%.3f\n",             // turbidity_r_mean_ftu
                 node_id, node_position, time_str, turbidity_aggs.reading_count,
                 turbidity_aggs.turbidity_s_mean_ftu, turbidity_aggs.turbidity_r_mean_ftu);
    if (log_buflen > 0) {
      BRIDGE_SENSOR_LOG_PRINTN(BM_COMMON_AGG, log_buf, log_buflen);
    } else {
      printf("ERROR: Failed to print Seapoint Turbidity aggregation to log\n");
    }
    reportBuilderAddToQueue(
        node_id, SENSOR_TYPE_SEAPOINT_TURBIDITY, static_cast<void *>(&turbidity_aggs),
        sizeof(seapoint_turbidity_aggregations_t), REPORT_BUILDER_SAMPLE_MESSAGE);
    memset(log_buf, 0, SENSOR_LOG_BUF_SIZE);
    // Clear the buffers
    turbidity_s_ftu.clear();
    turbidity_r_ftu.clear();
    reading_count = 0;
    xSemaphoreGive(_mutex);
  } else {
    printf("Failed to get the subbed Seapoint Turbidity mutex while trying to aggregate\n");
  }
  vPortFree(log_buf);
}

SeapointTurbidity_t *createSeapointTurbiditySub(uint64_t node_id, uint32_t agg_period_ms,
                                                uint32_t averager_max_samples) {
  SeapointTurbidity_t *new_sub =
      static_cast<SeapointTurbidity_t *>(pvPortMalloc(sizeof(SeapointTurbidity_t)));
  new_sub = new (new_sub) SeapointTurbidity_t();
  configASSERT(new_sub);

  new_sub->_mutex = xSemaphoreCreateMutex();
  configASSERT(new_sub->_mutex);

  new_sub->node_id = node_id;
  new_sub->type = SENSOR_TYPE_SEAPOINT_TURBIDITY;
  new_sub->next = NULL;
  new_sub->agg_period_ms = agg_period_ms;
  new_sub->turbidity_s_ftu.initBuffer(averager_max_samples);
  new_sub->turbidity_r_ftu.initBuffer(averager_max_samples);
  new_sub->reading_count = 0;
  return new_sub;
}
