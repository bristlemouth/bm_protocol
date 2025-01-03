#include "pmeDissolvedOxygenSensor.h"
#include "app_config.h"
#include "avgSampler.h"
#include "spotter.h"
#include "pubsub.h"
#include "pme_dissolved_oxygen_msg.h"
#include "bridgeLog.h"
#include "cbor.h"
#include "device_info.h"
#include "reportBuilder.h"
#include "semphr.h"
#include "stm32_rtc.h"
#include "topology_sampler.h"
#include "util.h"
#include <new>

#define DEFAULT_PME_DISSOLVED_READING_PERIOD_MS 10 * 60 * 1000 // 10 minutes

bool PmeDissolvedOxygenSensor::subscribe() {
  bool rval = false;
  char *sub = static_cast<char *>(pvPortMalloc(BM_TOPIC_MAX_LEN));
  configASSERT(sub);
  int topic_strlen =
      snprintf(sub, BM_TOPIC_MAX_LEN, "sensor/%016" PRIx64 "%s", node_id, subtag);
  if (topic_strlen > 0) {
    rval = bm_sub_wl(sub, topic_strlen, pmeDissolvedOxygenSubCallback);
  }
  vPortFree(sub);
  return rval;
}

void PmeDissolvedOxygenSensor::pmeDissolvedOxygenSubCallback(uint64_t node_id, const char *topic,
                                                             uint16_t topic_len, const uint8_t *data,
                                                             uint16_t data_len, uint8_t type, uint8_t version) {
  (void) type;
  (void) version;
  printf("PME Dissolved Oxygen data received from node %016" PRIx64 ", on topic: %.*s\n", node_id,
         topic_len, topic);
  PmeDissolvedOxygen_t *dissolved_oxygen_sensor =
      static_cast<PmeDissolvedOxygen_t *>(sensorControllerFindSensorById(node_id));
  if (dissolved_oxygen_sensor && dissolved_oxygen_sensor->type == SENSOR_TYPE_PME_DO) {
    if (xSemaphoreTake(dissolved_oxygen_sensor->_mutex, portMAX_DELAY)) {
      static PmeDissolvedOxygenMsg::Data dissolved_oxygen_data;
      if (PmeDissolvedOxygenMsg::decode(dissolved_oxygen_data, data, data_len) == CborNoError) {
        char *log_buff = static_cast<char *>(pvPortMalloc(SENSOR_LOG_BUF_SIZE));
        configASSERT(log_buff);
        dissolved_oxygen_sensor->temperature_deg_c.addSample(dissolved_oxygen_data.temperature_deg_c);
        dissolved_oxygen_sensor->do_mg_per_l.addSample(dissolved_oxygen_data.do_mg_per_l);
        dissolved_oxygen_sensor->quality.addSample(dissolved_oxygen_data.quality);
        dissolved_oxygen_sensor->do_saturation_pct.addSample(dissolved_oxygen_data.do_saturation_pct);
        dissolved_oxygen_sensor->reading_count++;
        // Large floats get formatted in scientific notation,
        // so we print integer seconds and millis separately.
        uint64_t reading_time_sec = dissolved_oxygen_data.header.reading_time_utc_ms / 1000U;
        uint32_t reading_time_millis = dissolved_oxygen_data.header.reading_time_utc_ms % 1000U;
        uint64_t sensor_reading_time_sec = dissolved_oxygen_data.header.sensor_reading_time_ms / 1000U;
        uint32_t sensor_reading_time_millis =
            dissolved_oxygen_data.header.sensor_reading_time_ms % 1000U;

        uint32_t current_timestamp = pdTICKS_TO_MS(xTaskGetTickCount());
        if (current_timestamp - dissolved_oxygen_sensor->last_timestamp >
            DEFAULT_PME_DISSOLVED_READING_PERIOD_MS + 1000U ||
            dissolved_oxygen_sensor->reading_count == 1U) {
          printf("Updating PME Dissolved Oxygen %016" PRIx64
                 " node position, current_time = %" PRIu32 ", last_time = %" PRIu32
                 ", reading count: %" PRIu32 "\n",
                 node_id, current_timestamp, dissolved_oxygen_sensor->last_timestamp,
                 dissolved_oxygen_sensor->reading_count);
          dissolved_oxygen_sensor->node_position =
              topology_sampler_get_node_position(node_id, pdTICKS_TO_MS(5000));
        }
        dissolved_oxygen_sensor->last_timestamp = current_timestamp;

        size_t log_buff_len =
            snprintf(log_buff, SENSOR_LOG_BUF_SIZE,
                     "%016" PRIx64 ","       // Node Id
                     "%" PRIi8 ","           // node_position
                     "pme_dissolved_oxygen," // node_app_name
                     "%" PRIu64 ","          // reading_uptime_millis
                     "%" PRIu64 "."          // reading_time_utc_ms seconds part
                     "%03" PRIu32 ","        // reading_time_utc_ms millis part
                     "%" PRIu64 ","          // sensor_reading_time_ms seconds part
                     "%03" PRIu32 ","        // sensor_reading_time_ms millis part
                     "%.4f,"                 // temperature_deg_c // TODO - check if this is the right format
                     "%.3f,"                 // do_mg_per_l
                     "%.3f,"                 // quality
                     "%.1f\n",               // do_saturation_pct
                     node_id, dissolved_oxygen_sensor->node_position,
                     dissolved_oxygen_data.header.reading_uptime_millis, reading_time_sec,
                     reading_time_millis, sensor_reading_time_sec, sensor_reading_time_millis,
                     dissolved_oxygen_data.temperature_deg_c, dissolved_oxygen_data.do_mg_per_l,
                     dissolved_oxygen_data.quality, dissolved_oxygen_data.do_saturation_pct);
        if (log_buff_len > 0) {
          BRIDGE_SENSOR_LOG_PRINTN(BM_COMMON_IND, log_buff, log_buff_len);
        } else {
          printf("ERROR: Failed to print PME Dissolved Oxygen data to log\n");
        }
        vPortFree(log_buff);
      }
      xSemaphoreGive(dissolved_oxygen_sensor->_mutex);
    } else {
      printf("Failed to take mutex for PME Dissolved Oxygen Sensor after getting a new reading\n");
    }
  }
}

void PmeDissolvedOxygenSensor::aggregate(void) {
  char *log_buff = static_cast<char *>(pvPortMalloc(SENSOR_LOG_BUF_SIZE));
  configASSERT(log_buff);
  if (xSemaphoreTake(_mutex, portMAX_DELAY)) {
    pme_dissolved_oxygen_aggregations_t dissolved_oxygen_aggs = {
      .temperature_deg_c_mean = NAN,
      .do_mg_per_l_mean = NAN,
      .quality_mean = NAN,
      .do_saturation_pct_mean = NAN,
      .reading_count = 0
    };

    if (temperature_deg_c.getNumSamples() > MIN_READINGS_FOR_AGGREGATION) {
      dissolved_oxygen_aggs.temperature_deg_c_mean = temperature_deg_c.getMean();
      dissolved_oxygen_aggs.do_mg_per_l_mean = do_mg_per_l.getMean();
      dissolved_oxygen_aggs.quality_mean = quality.getMean();
      dissolved_oxygen_aggs.do_saturation_pct_mean = do_saturation_pct.getMean();
      dissolved_oxygen_aggs.reading_count = reading_count;
    }

    static constexpr uint8_t TIME_STR_BUFSIZE = 50;
    char time_str[TIME_STR_BUFSIZE];
    if (!logRtcGetTimeStr(time_str, TIME_STR_BUFSIZE, true)) {
      printf("Failed to get RTC time string for PME Dissolved Oxygen aggregation\n");
      snprintf(time_str, TIME_STR_BUFSIZE, "0");
    }

    int8_t node_position = topology_sampler_get_node_position(node_id, pdTICKS_TO_MS(5000));

    size_t log_buflen =
        snprintf(log_buff, SENSOR_LOG_BUF_SIZE,
                 "%016" PRIx64 ","     // Node Id
                 "%" PRIi8 ","         // node_position
                 "pme_dissolved_oxygen," // node_app_name
                 "%s,"                 // timestamp(ticks/UTC)
                 "%" PRIu32 ","        // reading_count
                 "%.4f,"               // temperature_deg_c
                 "%.3f,"               // do_mg_per_l
                 "%.3f,"               // quality
                 "%.1f\n",             // do_saturation_pct
                 node_id, node_position, time_str, dissolved_oxygen_aggs.reading_count,
                 dissolved_oxygen_aggs.temperature_deg_c_mean, dissolved_oxygen_aggs.do_mg_per_l_mean,
                 dissolved_oxygen_aggs.quality_mean, dissolved_oxygen_aggs.do_saturation_pct_mean);
    if (log_buflen > 0) {
      BRIDGE_SENSOR_LOG_PRINTN(BM_COMMON_AGG, log_buff, log_buflen);
    } else {
      printf("ERROR: Failed to print PME Dissolved Oxygen aggregation to log\n");
    }
    reportBuilderAddToQueue(
        node_id, SENSOR_TYPE_PME_DO, static_cast<void *>(&dissolved_oxygen_aggs),
        sizeof(pme_dissolved_oxygen_aggregations_t), REPORT_BUILDER_SAMPLE_MESSAGE);
    memset(log_buff, 0, SENSOR_LOG_BUF_SIZE);
    // Clear the buffers
    temperature_deg_c.clear();
    do_mg_per_l.clear();
    quality.clear();
    do_saturation_pct.clear();
    reading_count = 0;
    xSemaphoreGive(_mutex);
  } else {
    printf("Failed to take mutex for PME Dissolved Oxygen Sensor after getting a new reading\n");
  }
  vPortFree(log_buff);
}

PmeDissolvedOxygen_t *createPmeDissolvedOxygenSub(uint64_t node_id, uint32_t agg_period_ms,
                                                  uint32_t averager_max_samples) {
  PmeDissolvedOxygen_t *new_sub = static_cast<PmeDissolvedOxygen_t *>(pvPortMalloc(sizeof(PmeDissolvedOxygen_t)));
  new_sub = new (new_sub) PmeDissolvedOxygen_t();
  configASSERT(new_sub);

  new_sub->_mutex = xSemaphoreCreateMutex();
  configASSERT(new_sub->_mutex);

  new_sub->node_id = node_id;
  new_sub->type = SENSOR_TYPE_PME_DO;
  new_sub->next = NULL;
  new_sub->agg_period_ms = agg_period_ms;
  new_sub->temperature_deg_c.initBuffer(averager_max_samples);
  new_sub->do_mg_per_l.initBuffer(averager_max_samples);
  new_sub->quality.initBuffer(averager_max_samples);
  new_sub->do_saturation_pct.initBuffer(averager_max_samples);
  new_sub->reading_count = 0;
  return new_sub;
}