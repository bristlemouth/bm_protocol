#include "pmeWipeSensor.h"
#include "FreeRTOS.h"
#include "app_config.h"
#include "avgSampler.h"
#include "bm_config.h"
#include "bm_os.h"
#include "bridgeLog.h"
#include "cbor.h"
#include "device_info.h"
#include "pme_wipe_msg.h"
#include "pubsub.h"
#include "reportBuilder.h"
#include "semphr.h"
#include "spotter.h"
#include "stm32_rtc.h"
#include "topology_sampler.h"
#include "util.h"
#include <new>

#define DEFAULT_PME_WIPER_READING_PERIOD_MS 10 * 60 * 1000 // 10 minutes

bool PmeWipeSensor::subscribe() {
  bool rval = false;
  char *sub = static_cast<char *>(bm_malloc(BM_TOPIC_MAX_LEN));
  if (sub) {
    int topic_strlen =
        snprintf(sub, BM_TOPIC_MAX_LEN, "sensor/%016" PRIx64 "%s", node_id, subtag);
    if (topic_strlen > 0) {
      rval = (bm_sub_wl(sub, topic_strlen, pmeWipeSubCallback) == BmOK);
    }
    bm_free(sub);
  }
  return rval;
}

void PmeWipeSensor::pmeWipeSubCallback(uint64_t node_id, const char *topic, uint16_t topic_len,
                                       const uint8_t *data, uint16_t data_len, uint8_t type,
                                       uint8_t version) {
  (void)type;
  (void)version;
  bm_debug("PME Wiper data received from node %016" PRIx64 ", on topic: %.*s\n", node_id,
           topic_len, topic);
  PmeWipe_t *wipe_sensor = static_cast<PmeWipe_t *>(sensorControllerFindSensorById(node_id));
  if (wipe_sensor && wipe_sensor->type == SENSOR_TYPE_PME_WIPE && wipe_sensor->_mutex) {
    if (bm_semaphore_take(wipe_sensor->_mutex, BM_MAX_DELAY_UINT32) == BmOK) {
      static PmeWipeMsg::Data wipe_data;
      if (PmeWipeMsg::decode(wipe_data, data, data_len) == CborNoError) {
        char *log_buff = static_cast<char *>(bm_malloc(SENSOR_LOG_BUF_SIZE));
        configASSERT(log_buff);
        wipe_sensor->wipe_current_ma.addSample(wipe_data.wipe_current_mean_ma);
        wipe_sensor->wipe_duration_s.addSample(wipe_data.wipe_duration_s);
        wipe_sensor->reading_count++;
        // Large floats get formatted in scientific notation,
        // so we print integer seconds and millis separately.
        uint64_t reading_time_sec = wipe_data.header.reading_time_utc_ms / 1000U;
        uint32_t reading_time_millis = wipe_data.header.reading_time_utc_ms % 1000U;
        uint64_t sensor_reading_time_sec = wipe_data.header.sensor_reading_time_ms / 1000U;
        uint32_t sensor_reading_time_millis = wipe_data.header.sensor_reading_time_ms % 1000U;

        uint32_t current_timestamp = pdTICKS_TO_MS(xTaskGetTickCount());
        if (current_timestamp - wipe_sensor->last_timestamp >
                DEFAULT_PME_WIPER_READING_PERIOD_MS + 1000U ||
            wipe_sensor->reading_count == 1U) {
          bm_debug("Updating PME Wiper %016" PRIx64 " node position, current_time = %" PRIu32
                   ", last_time = %" PRIu32 ", reading count: %" PRIu32 "\n",
                   node_id, current_timestamp, wipe_sensor->last_timestamp,
                   wipe_sensor->reading_count);
          wipe_sensor->node_position =
              topology_sampler_get_node_position(node_id, pdTICKS_TO_MS(5000));
        }
        wipe_sensor->last_timestamp = current_timestamp;

        size_t log_buff_len =
            snprintf(log_buff, SENSOR_LOG_BUF_SIZE,
                     "%016" PRIx64 ","       // Node Id
                     "%" PRIi8 ","           // node_position
                     "pme_wiper,"            // node_app_name
                     "%" PRIu64 ","          // reading_uptime_millis
                     "%" PRIu64 "."          // reading_time_utc_ms seconds part
                     "%03" PRIu32 ","        // reading_time_utc_ms millis part
                     "%" PRIu64 ","          // sensor_reading_time_ms seconds part
                     "%03" PRIu32 ","        // sensor_reading_time_ms millis part
                     "%.3f,"                 // Wipe current mean
                     "%.3f\n",               // Wipe duration mean
                     node_id, wipe_sensor->node_position,
                     wipe_data.header.reading_uptime_millis, reading_time_sec,
                     reading_time_millis, sensor_reading_time_sec, sensor_reading_time_millis,
                     wipe_data.wipe_current_mean_ma, wipe_data.wipe_duration_s);
        if (log_buff_len > 0) {
          BRIDGE_SENSOR_LOG_PRINTN(BM_COMMON_IND, log_buff, log_buff_len);
        } else {
          bm_debug("ERROR: Failed to print PME Wiper data to log\n");
        }
        bm_free(log_buff);
      }
      bm_semaphore_give(wipe_sensor->_mutex);
    } else {
      bm_debug("Failed to take mutex for PME Wiper Sensor after getting a new reading\n");
    }
  }
}

void PmeWipeSensor::aggregate(void) {
  char *log_buff = static_cast<char *>(bm_malloc(SENSOR_LOG_BUF_SIZE));
  configASSERT(log_buff);
  if (bm_semaphore_take(_mutex, BM_MAX_DELAY_UINT32) == BmOK) {
    pme_wipe_aggregations_t wipe_aggs = {
        .wipe_current_mean_ma = NAN,
        .wipe_duration_s = NAN,
        .reading_count = reading_count,
    };

    if (wipe_current_ma.getNumSamples() >= MIN_READINGS_FOR_AGGREGATION) {
      wipe_aggs.wipe_current_mean_ma = wipe_current_ma.getMean();
      wipe_aggs.wipe_duration_s = wipe_duration_s.getMean();
      wipe_aggs.reading_count = reading_count;
    }

    static constexpr uint8_t TIME_STR_BUFSIZE = 50;
    char time_str[TIME_STR_BUFSIZE];
    if (!logRtcGetTimeStr(time_str, TIME_STR_BUFSIZE, true)) {
      bm_debug("Failed to get time string for PME Wiper Sensor\n");
      snprintf(time_str, TIME_STR_BUFSIZE, "0");
    }

    int8_t node_position = topology_sampler_get_node_position(node_id, 5000);

    size_t log_buflen = snprintf(log_buff, SENSOR_LOG_BUF_SIZE,
                                 "%016" PRIx64 ","       // Node Id
                                 "%" PRIi8 ","           // node_position
                                 "pme_wiper,"            // node_app_name
                                 "%s,"                   // timestamp(ticks/UTC)
                                 "%" PRIu32 ","          // reading_count
                                 "%.4f,"                 // temperature_deg_c
                                 "%.3f\n",               // do_mg_per_l
                                 node_id, node_position, time_str, wipe_aggs.reading_count,
                                 wipe_aggs.wipe_current_mean_ma, wipe_aggs.wipe_duration_s);

    if (log_buflen > 0) {
      BRIDGE_SENSOR_LOG_PRINTN(BM_COMMON_IND, log_buff, log_buflen);
    } else {
      bm_debug("ERROR: Failed to print PME Wiper data to log\n");
    }

    // TODO - figure out if we want to add to the report builder and how we will do it!
    // reportBuilderAddToQueue(node_id, SENSOR_TYPE_PME_WIPE, static_cast<void *>(&wipe_aggs),
    //                         sizeof(pme_wipe_aggregations_t), REPORT_BUILDER_SAMPLE_MESSAGE);

    memset(log_buff, 0, SENSOR_LOG_BUF_SIZE);
    wipe_current_ma.clear();
    wipe_duration_s.clear();
    reading_count = 0;
    bm_semaphore_give(_mutex);
  } else {
    bm_debug("Failed to take mutex for PME Wiper Sensor after getting a new reading\n");
  }
  bm_free(log_buff);
}

PmeWipe_t *createPmeWipeSub(uint64_t node_id, uint32_t agg_period_ms,
                            uint32_t averager_max_samples) {
  PmeWipe_t *new_sub = static_cast<PmeWipe_t *>(bm_malloc(sizeof(PmeWipe_t)));
  if (new_sub) {
    new_sub = new (new_sub) PmeWipe_t();

    new_sub->_mutex = xSemaphoreCreateMutex();

    if (new_sub->_mutex) {
      new_sub->node_id = node_id;
      new_sub->type = SENSOR_TYPE_PME_WIPE;
      new_sub->next = NULL;
      new_sub->agg_period_ms = agg_period_ms;
      new_sub->wipe_current_ma.initBuffer(averager_max_samples);
      new_sub->wipe_duration_s.initBuffer(averager_max_samples);
      new_sub->reading_count = 0;
    } else {
      bm_free(new_sub);
      new_sub = NULL;
    }
  }
  return new_sub;
}
