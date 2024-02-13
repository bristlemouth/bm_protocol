#include "rbrCodaSensor.h"
#include "app_config.h"
#include "avgSampler.h"
#include "bm_network.h"
#include "bm_pubsub.h"
#include "bm_rbr_data_msg.h"
#include "bridgeLog.h"
#include "device_info.h"
#include "reportBuilder.h"
#include "semphr.h"
#include "stm32_rtc.h"
#include "topology_sampler.h"
#include "util.h"
#include <new>

bool RbrCodaSensor::subscribe() {
  bool rval = false;
  char *sub = static_cast<char *>(pvPortMalloc(BM_TOPIC_MAX_LEN));
  configASSERT(sub);
  int topic_strlen = snprintf(sub, BM_TOPIC_MAX_LEN, "%" PRIx64 "%s", node_id, subtag);
  if (topic_strlen > 0) {
    rval = bm_sub_wl(sub, topic_strlen, rbrCodaSubCallback);
  }
  vPortFree(sub);
  return
}

void RbrCodaSensor::rbrCodaSubCallback(uint64_t node_id, const char *topic, uint16_t topic_len,
                                       const uint8_t *data, uint16_t data_len, uint8_t type,
                                       uint8_t version) {
  (void)type;
  (void)version;
  printf("RBR CODA data received from node %" PRIx64 " On topic: %.*s\n", node_id, topic_len,
        topic);
  RbrCoda_t *rbr_coda = static_cast<RbrCoda_t *>(sensorControllerFindSensorById(node_id));
  if (rbr_coda && rbr_coda->type == SENSOR_TYPE_RBR) {
    if (xSemaphoreTake(rbr_coda->_mutex, portMAX_DELAY)) {
      static BmRbrDataMsg::Data rbr_data;
      if (BmRbrDataMsg::decode(rbr_data, data, data_len) == CborNoError) {
        char *log_buf = static_cast<char *>(pvPortMalloc(SENSOR_LOG_BUF_SIZE));
        configAssert(log_buf);
        rbr_coda->temp_deg_c.addSample(rbr_data.temperature_deg_c);
        rbr_coda->pressure_ubar.addSample(rbr_data.pressure_ubar);
        rbr_coda->reading_count++;

        // Large floats get formatted in scientific notation,
        // so we print integer seconds and millis separately.
        uint64_t reading_time_sec = rbr_data.header.reading_time_utc_ms / 1000U;
        uint32_t reading_time_millis = rbr_data.header.reading_time_utc_ms % 1000U;
        uint64_t sensor_reading_time_sec = rbr_data.header.sensor_reading_time_ms / 1000U;
        uint32_t sensor_reading_time_millis = rbr_data.header.sensor_reading_time_ms % 1000U;

        int8_t node_position = topology_sampler_get_node_position(node_id, 1000);

        size_t log_buflen =
            snprintf(log_buf, SENSOR_LOG_BUF_SIZE,
                     "%" PRIx64 ","   // Node Id
                     "%" PRIi8 ","    // node_position
                     "rbr_coda,"      // node_app_name
                     "%" PRIu64 ","   // reading_uptime_millis
                     "%" PRIu64 "."   // reading_time_utc_ms seconds part
                     "%03" PRIu32 "," // reading_time_utc_ms millis part
                     "%" PRIu64 ","   // sensor_reading_time_ms seconds part
                     "%03" PRIu32 "," // sensor_reading_time_ms millis part
                     "%.2f,"          // temperature_deg_c
                     "%.2f,"          // pressure_ubar
                     "%" PRIu32 "\n", // reading_count
                     node_id, node_position, rbr_data.header.reading_uptime_ms, reading_time_sec,
                     reading_time_millis, sensor_reading_time_sec, sensor_reading_time_millis,
                     rbr_data.temperature_deg_c, rbr_data.pressure_ubar, rbr_coda->reading_count);
        if (log_buflen > 0) {
          BRIDGE_SENSOR_LOG_PRINTN(BM_COMMON_IND, log_buf, log_buflen);
        } else {
          printf("ERROR: Failed to print rbr_coda individual log\n");
        }
        vPortFree(log_buf);
      }
      xSemaphoreGive(rbr_coda->_mutex);
    } else {
      printf("Failed to get the subbed RBR CODA mutex after getting a new reading\n");
    }
  }
}

void RbrCodaSensor::aggregate(void) {
  char *log_buf = static_cast<char *>(pvPortMalloc(SENSOR_LOG_BUF_SIZE));
  configAssert(log_buf);
  if (xSemaphoreTake(_mutex, portMAX_DELAY)) {
    size_t log_buflen = 0;
    rbr_coda_aggregations_t aggs = {.temp_mean_deg_c = NAN,
                                    .pressure_mean_ubar = NAN,
                                    .pressure_stdev_ubar = NAN,
                                    .reading_count = 0};
    if (temp_deg_c.getNumSamples() >= MIN_READINGS_FOR_AGGREGATION) {
      aggs.temp_mean_deg_c = temp_deg_c.getMean();
      aggs.pressure_mean_ubar = pressure_ubar.getMean();
      aggs.pressure_stdev_ubar = pressure_ubar.getStd(aggs.pressure_mean_ubar);
      aggs.reading_count = reading_count;

      if (aggs.temp_mean_deg_c < TEMP_SAMPLE_MEMBER_MIN ||
          aggs.temp_mean_deg_c > TEMP_SAMPLE_MEMBER_MAX) {
        aggs.temp_mean_deg_c = NAN;
      }
      if (aggs.pressure_mean_ubar < PRESSURE_SAMPLE_MEMBER_MIN ||
          aggs.pressure_mean_ubar > PRESSURE_SAMPLE_MEMBER_MAX) {
        aggs.pressure_mean_ubar = NAN;
        aggs.pressure_stdev_ubar = NAN;
      }
    }
    static constexpr uint8_t TIME_STR_BUFSIZE = 50;
    char time_str[TIME_STR_BUFSIZE];
    if (!logRtcGetTimeStr(time_str, TIME_STR_BUFSIZE, true)) {
      printf("Failed to get RTC time string for RBR CODA aggregation\n");
      snprintf(time_str, TIME_STR_BUFSIZE, "0");
    }

    int8_t node_position = topology_sampler_get_node_position(node_id, 1000);

    log_buflen =
        snprintf(log_buf, SENSOR_LOG_BUF_SIZE,
                 "%" PRIx64 "," // Node Id
                 "%" PRIi8 ","  // node_position
                 "rbr_coda,"    // node_app_name
                 "%" PRIu64 "," // reading_uptime_millis
                 "%s,"          // time_str
                 "%.2f,"         // temp_mean_deg_c
                 "%.2f,"         // pressure_mean_ubar
                 "%.2f,"         // pressure_stdev_ubar
                 "%" PRIu32 "\n", // reading_count
                 node_id, node_position, aggs.reading_uptime_millis, time_str, aggs.temp_mean_deg_c,
                 aggs.pressure_mean_ubar, aggs.pressure_stdev_ubar, aggs.reading_count);
    if (log_buflen > 0) {
      BRIDGE_SENSOR_LOG_PRINTN(BM_COMMON_IND, log_buf, log_buflen);
    } else {
      printf("ERROR: Failed to print rbr_coda aggregation log\n");
    }
    reportBuilderAddToQueue(node_id, SENSOR_TYPE_RBR, static_cast<void *>(&aggs),
                            sizeof(rbr_coda_aggregations_t), REPORT_BUILDER_SAMPLE_MESSAGE);
    temp_deg_c.clear();
    pressure_ubar.clear();
    reading_count = 0;
    xSemaphoreGive(_mutex);
  } else {
    printf("Failed to get the RBR CODA mutex after getting a new reading\n");
  }
  vPortFree(log_buf);
}

RbrCoda_t* createRbrCodaSub(uint64_t node_id, uint32_t rbr_coda_agg_period_ms,
                            uint32_t averager_max_samples) {
  RbrCoda_t *new_sub = static_cast<RbrCoda_t *>(pvPortMalloc(sizeof(RbrCoda_t)));
  new_sub = new (std::nothrow) RbrCoda_t;
  configAssert(new_sub);

  new_sub->_mutex = xSemaphoreCreateMutex();
  configAssert(new_sub->_mutex);

  new_sub->node_id = node_id;
  new_sub->type = SENSOR_TYPE_RBR;
  new_sub->next = NULL;
  new_sub->rbr_coda_agg_period_ms = rbr_coda_agg_period_ms;
  new_sub->temp_deg_c.init(averager_max_samples);
  new_sub->pressure_ubar.init(averager_max_samples);
  new_sub->reading_count = 0;
  return new_sub;
}