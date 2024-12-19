#include "softSensor.h"
#include "app_config.h"
#include "avgSampler.h"
#include "spotter.h"
#include "pubsub.h"
#include "bm_soft_data_msg.h"
#include "bridgeLog.h"
#include "device_info.h"
#include "reportBuilder.h"
#include "semphr.h"
#include "stm32_rtc.h"
#include "topology_sampler.h"
#include "app_util.h"
#include <new>

// TODO - get this from the sensor node itself
#define DEFAULT_SOFT_READING_PERIOD_MS 500 // 2Hz

bool SoftSensor::subscribe() {
  bool rval = false;
  char *sub = static_cast<char *>(pvPortMalloc(BM_TOPIC_MAX_LEN));
  configASSERT(sub);
  int topic_strlen =
      snprintf(sub, BM_TOPIC_MAX_LEN, "sensor/%016" PRIx64 "%s", node_id, subtag);
  if (topic_strlen > 0) {
    rval = bm_sub_wl(sub, topic_strlen, softSubCallback) == BmOK;
  }
  vPortFree(sub);
  return rval;
}

void SoftSensor::softSubCallback(uint64_t node_id, const char *topic, uint16_t topic_len,
                                 const uint8_t *data, uint16_t data_len, uint8_t type,
                                 uint8_t version) {
  (void)type;
  (void)version;
  printf("SOFT data received from node %016" PRIx64 " On topic: %.*s\n", node_id, topic_len,
         topic);
  Soft_t *soft = static_cast<Soft_t *>(sensorControllerFindSensorById(node_id));
  if (soft && soft->type == SENSOR_TYPE_SOFT) {
    if (xSemaphoreTake(soft->_mutex, portMAX_DELAY)) {
      static BmSoftDataMsg::Data soft_data;
      if (BmSoftDataMsg::decode(soft_data, data, data_len) == CborNoError) {
        char *log_buf = static_cast<char *>(pvPortMalloc(SENSOR_LOG_BUF_SIZE));
        configASSERT(log_buf);
        soft->temp_deg_c.addSample(soft_data.temperature_deg_c);
        soft->reading_count++;

        // Large floats get formatted in scientific notation,
        // so we print integer seconds and millis separately.
        uint64_t reading_time_sec = soft_data.header.reading_time_utc_ms / 1000U;
        uint32_t reading_time_millis = soft_data.header.reading_time_utc_ms % 1000U;
        uint64_t sensor_reading_time_sec = soft_data.header.sensor_reading_time_ms / 1000U;
        uint32_t sensor_reading_time_millis = soft_data.header.sensor_reading_time_ms % 1000U;

        uint32_t current_timestamp = pdTICKS_TO_MS(xTaskGetTickCount());
        if ((current_timestamp - soft->last_timestamp > DEFAULT_SOFT_READING_PERIOD_MS + 1000u) ||
            soft->reading_count == 1U) {
          printf("Updating soft %016" PRIx64 " node position, current_time = %" PRIu32
                 ", last_time = %" PRIu32 ", reading count: %" PRIu32 "\n",
                 node_id, current_timestamp, soft->last_timestamp, soft->reading_count);
          soft->node_position = topology_sampler_get_node_position(node_id, pdTICKS_TO_MS(5000));
        }
        soft->last_timestamp = current_timestamp;

        size_t log_buflen =
            snprintf(log_buf, SENSOR_LOG_BUF_SIZE,
                     "%016" PRIx64 "," // Node Id
                     "%" PRIi8 ","     // node_position
                     "soft,"           // node_app_name
                     "%" PRIu64 ","    // reading_uptime_millis
                     "%" PRIu64 "."    // reading_time_utc_ms seconds part
                     "%03" PRIu32 ","  // reading_time_utc_ms millis part
                     "%" PRIu64 "."    // sensor_reading_time_ms seconds part
                     "%03" PRIu32 ","  // sensor_reading_time_ms millis part
                     "%.3f\n",         // temp_deg_c
                     node_id, soft->node_position, soft_data.header.reading_uptime_millis,
                     reading_time_sec, reading_time_millis, sensor_reading_time_sec,
                     sensor_reading_time_millis, soft_data.temperature_deg_c);
        if (log_buflen > 0) {
          BRIDGE_SENSOR_LOG_PRINTN(BM_COMMON_IND, log_buf, log_buflen);
        } else {
          printf("ERROR: Failed to print soft individual log\n");
        }
        vPortFree(log_buf);
      }
      xSemaphoreGive(soft->_mutex);
    } else {
      printf("Failed to get the subbed Soft mutex after getting a new reading\n");
    }
  }
}

void SoftSensor::aggregate(void) {
  char *log_buf = static_cast<char *>(pvPortMalloc(SENSOR_LOG_BUF_SIZE));
  configASSERT(log_buf);
  if (xSemaphoreTake(_mutex, portMAX_DELAY)) {
    size_t log_buflen = 0;
    soft_aggregations_t soft_aggs = {.temp_mean_deg_c = NAN, .reading_count = 0};
    // Check to make sure we have enough sensor readings for a valid aggregation.
    // If not send NaNs for all the values.
    if (temp_deg_c.getNumSamples() >= MIN_READINGS_FOR_AGGREGATION) {
      soft_aggs.temp_mean_deg_c = temp_deg_c.getMean();
      soft_aggs.reading_count = reading_count;
      if (soft_aggs.temp_mean_deg_c < TEMP_SAMPLE_MEMBER_MIN) {
        soft_aggs.temp_mean_deg_c = -HUGE_VAL;
      } else if (soft_aggs.temp_mean_deg_c > TEMP_SAMPLE_MEMBER_MAX) {
        soft_aggs.temp_mean_deg_c = HUGE_VAL;
      }
    }
    static constexpr uint8_t TIME_STR_BUFSIZE = 50;
    char time_str[TIME_STR_BUFSIZE];
    if (!logRtcGetTimeStr(time_str, TIME_STR_BUFSIZE, true)) {
      printf("Failed to get RTC time string for Soft aggregation\n");
      snprintf(time_str, TIME_STR_BUFSIZE, "0");
    }

    int8_t node_position = topology_sampler_get_node_position(node_id, pdTICKS_TO_MS(5000));

    log_buflen = snprintf(log_buf, SENSOR_LOG_BUF_SIZE,
                          "%016" PRIx64 "," // Node Id
                          "%" PRIi8 ","     // node_position
                          "soft,"           // node_app_name
                          "%s,"             // timestamp(ticks/UTC)
                          "%" PRIu32 ","    // reading_count
                          "%.3f\n",         // temp_mean_deg_c
                          node_id, node_position, time_str, soft_aggs.reading_count,
                          soft_aggs.temp_mean_deg_c);
    if (log_buflen > 0) {
      BRIDGE_SENSOR_LOG_PRINTN(BM_COMMON_AGG, log_buf, log_buflen);
    } else {
      printf("ERROR: Failed to print soft aggregate log\n");
    }
    reportBuilderAddToQueue(node_id, SENSOR_TYPE_SOFT, static_cast<void *>(&soft_aggs),
                            sizeof(soft_aggregations_t), REPORT_BUILDER_SAMPLE_MESSAGE);
    memset(log_buf, 0, SENSOR_LOG_BUF_SIZE);
    // Clear the buffers
    temp_deg_c.clear();
    reading_count = 0;
    xSemaphoreGive(_mutex);
  } else {
    printf("Failed to get the subbed Soft mutex while trying to aggregate\n");
  }
  vPortFree(log_buf);
}

Soft_t *createSoftSub(uint64_t node_id, uint32_t current_agg_period_ms,
                      uint32_t averager_max_samples) {
  Soft_t *new_sub = static_cast<Soft_t *>(pvPortMalloc(sizeof(Soft_t)));
  new_sub = new (new_sub) Soft_t();
  configASSERT(new_sub);

  new_sub->_mutex = xSemaphoreCreateMutex();
  configASSERT(new_sub->_mutex);

  new_sub->node_id = node_id;
  new_sub->type = SENSOR_TYPE_SOFT;
  new_sub->next = NULL;
  new_sub->current_agg_period_ms = current_agg_period_ms;
  new_sub->temp_deg_c.initBuffer(averager_max_samples);
  new_sub->reading_count = 0;
  return new_sub;
}
