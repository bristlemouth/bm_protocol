#include "aanderaaSensor.h"
#include "aanderaa_data_msg.h"
#include "app_config.h"
#include "avgSampler.h"
#include "spotter.h"
#include "pubsub.h"
#include "bridgeLog.h"
#include "device_info.h"
#include "reportBuilder.h"
#include "semphr.h"
#include "stm32_rtc.h"
#include "topology_sampler.h"
#include "app_util.h"
#include <new>

// TODO - get this from the sensor node itself
#define DEFAULT_AANDERAA_READING_PERIOD_MS 60 * 1000 // 1 minute

bool AanderaaSensor::subscribe() {
  bool rval = false;
  char *sub = static_cast<char *>(pvPortMalloc(BM_TOPIC_MAX_LEN));
  configASSERT(sub);
  int topic_strlen =
      snprintf(sub, BM_TOPIC_MAX_LEN, "sensor/%016" PRIx64 "%s", node_id, subtag);
  if (topic_strlen > 0) {
    rval = bm_sub_wl(sub, topic_strlen, aanderaSubCallback) == BmOK;
  }
  vPortFree(sub);
  return rval;
}

void AanderaaSensor::aanderaSubCallback(uint64_t node_id, const char *topic, uint16_t topic_len,
                                        const uint8_t *data, uint16_t data_len, uint8_t type,
                                        uint8_t version) {
  (void)type;
  (void)version;
  printf("Aanderaa data received from node %016" PRIx64 " On topic: %.*s\n", node_id, topic_len,
         topic);
  Aanderaa_t *aanderaa = static_cast<Aanderaa_t *>(sensorControllerFindSensorById(node_id));
  if (aanderaa && aanderaa->type == SENSOR_TYPE_AANDERAA) {
    if (xSemaphoreTake(aanderaa->_mutex, portMAX_DELAY)) {
      static AanderaaDataMsg::Data d;
      if (AanderaaDataMsg::decode(d, data, data_len) == CborNoError) {
        char *log_buf = static_cast<char *>(pvPortMalloc(SENSOR_LOG_BUF_SIZE));
        configASSERT(log_buf);
        aanderaa->abs_speed_cm_s.addSample(d.abs_speed_cm_s);
        aanderaa->direction_rad.addSample(degToRad(d.direction_deg_m));
        aanderaa->temp_deg_c.addSample(d.temperature_deg_c);
        aanderaa->abs_tilt_rad.addSample(degToRad(d.abs_tilt_deg));
        aanderaa->std_tilt_rad.addSample(degToRad(d.std_tilt_deg));
        aanderaa->reading_count++;

        // Large floats get formatted in scientific notation,
        // so we print integer seconds and millis separately.
        uint64_t reading_time_sec = d.header.reading_time_utc_ms / 1000U;
        uint32_t reading_time_millis = d.header.reading_time_utc_ms % 1000U;
        uint64_t sensor_reading_time_sec = d.header.sensor_reading_time_ms / 1000U;
        uint32_t sensor_reading_time_millis = d.header.sensor_reading_time_ms % 1000U;

        uint32_t current_timestamp = pdTICKS_TO_MS(xTaskGetTickCount());
        if ((current_timestamp - aanderaa->last_timestamp > DEFAULT_AANDERAA_READING_PERIOD_MS + 30000u) ||
            aanderaa->reading_count == 1U) {
          printf("Updating aanderaa %016" PRIx64 " node position, current_time = %" PRIu32
                 ", last_time = %" PRIu32 ", reading count: %" PRIu32 "\n",
                 node_id, current_timestamp, aanderaa->last_timestamp, aanderaa->reading_count);
          aanderaa->node_position = topology_sampler_get_node_position(node_id, pdTICKS_TO_MS(5000));
        }
        aanderaa->last_timestamp = current_timestamp;

        size_t log_buflen = snprintf(
            log_buf, SENSOR_LOG_BUF_SIZE,
            "%016" PRIx64 "," // Node Id
            "%" PRIi8 ","     // node_position
            "aanderaa,"       // node_app_name
            "%" PRIu64 ","    // reading_uptime_millis
            "%" PRIu64 "."    // reading_time_utc_ms seconds part
            "%03" PRIu32 ","  // reading_time_utc_ms millis part
            "%" PRIu64 "."    // sensor_reading_time_ms seconds part
            "%03" PRIu32 ","  // sensor_reading_time_ms millis part
            "%.3f,"           // abs_speed_cm_s
            "%.3f,"           // direction_deg_m
            "%.3f,"           // north_cm_s
            "%.3f,"           // east_cm_s
            "%.3f,"           // heading_deg_m
            "%.3f,"           // tilt_x_deg
            "%.3f,"           // tilt_y_deg
            "%.3f,"           // single_ping_std_cm_s
            "%.3f,"           // transducer_strength_db
            "%.3f,"           // ping_count
            "%.3f,"           // abs_tilt_deg
            "%.3f,"           // max_tilt_deg
            "%.3f,"           // std_tilt_deg
            "%.3f\n",         // temperature_deg_c
            node_id, aanderaa->node_position, d.header.reading_uptime_millis, reading_time_sec,
            reading_time_millis, sensor_reading_time_sec, sensor_reading_time_millis,
            d.abs_speed_cm_s, d.direction_deg_m, d.north_cm_s, d.east_cm_s, d.heading_deg_m,
            d.tilt_x_deg, d.tilt_y_deg, d.single_ping_std_cm_s, d.transducer_strength_db,
            d.ping_count, d.abs_tilt_deg, d.max_tilt_deg, d.std_tilt_deg, d.temperature_deg_c);
        if (log_buflen > 0) {
          BRIDGE_SENSOR_LOG_PRINTN(BM_COMMON_IND, log_buf, log_buflen);
        } else {
          printf("ERROR: Failed to print Aanderaa data\n");
        }
        vPortFree(log_buf);
      }
      xSemaphoreGive(aanderaa->_mutex);
    } else {
      printf("Failed to get the subbed Aanderaa mutex after getting a new reading\n");
    }
  }
}

void AanderaaSensor::aggregate(void) {
  char *log_buf = static_cast<char *>(pvPortMalloc(SENSOR_LOG_BUF_SIZE));
  configASSERT(log_buf);
  if (xSemaphoreTake(_mutex, portMAX_DELAY)) {
    size_t log_buflen = 0;
    aanderaa_aggregations_t agg = {.abs_speed_mean_cm_s = NAN,
                                   .abs_speed_std_cm_s = NAN,
                                   .direction_circ_mean_rad = NAN,
                                   .direction_circ_std_rad = NAN,
                                   .temp_mean_deg_c = NAN,
                                   .abs_tilt_mean_rad = NAN,
                                   .std_tilt_mean_rad = NAN,
                                   .reading_count = 0};
    // Check to make sure we have enough sensor readings for a valid aggregation.
    // If not send NaNs for all the values.
    // TODO - verify that we can assume if one sampler is below the min then all of them are.
    if (abs_speed_cm_s.getNumSamples() >= MIN_READINGS_FOR_AGGREGATION) {
      agg.abs_speed_mean_cm_s = abs_speed_cm_s.getMean(true);
      agg.abs_speed_std_cm_s = abs_speed_cm_s.getStd(agg.abs_speed_mean_cm_s, 0.0, true);
      agg.direction_circ_mean_rad = direction_rad.getCircularMean();
      agg.direction_circ_std_rad = direction_rad.getCircularStd();
      agg.temp_mean_deg_c = temp_deg_c.getMean(true);
      agg.abs_tilt_mean_rad = abs_tilt_rad.getMean(true);
      agg.std_tilt_mean_rad = std_tilt_rad.getMean(true);
      agg.reading_count = reading_count;
      if (agg.abs_speed_mean_cm_s < ABS_SPEED_SAMPLE_MEMBER_MIN ||
          agg.abs_speed_mean_cm_s > ABS_SPEED_SAMPLE_MEMBER_MAX) {
        agg.abs_speed_mean_cm_s = NAN;
      }
      if (agg.abs_speed_std_cm_s < ABS_SPEED_SAMPLE_MEMBER_MIN ||
          agg.abs_speed_std_cm_s > ABS_SPEED_SAMPLE_MEMBER_MAX) {
        agg.abs_speed_std_cm_s = NAN;
      }
      if (agg.direction_circ_mean_rad < DIRECTION_SAMPLE_MEMBER_MIN ||
          agg.direction_circ_mean_rad > DIRECTION_SAMPLE_MEMBER_MAX) {
        agg.direction_circ_mean_rad = NAN;
      }
      if (agg.direction_circ_std_rad < DIRECTION_SAMPLE_MEMBER_MIN ||
          agg.direction_circ_std_rad > DIRECTION_SAMPLE_MEMBER_MAX) {
        agg.direction_circ_std_rad = NAN;
      }
      if (agg.temp_mean_deg_c < TEMP_SAMPLE_MEMBER_MIN ||
          agg.temp_mean_deg_c > TEMP_SAMPLE_MEMBER_MAX) {
        agg.temp_mean_deg_c = NAN;
      }
      if (agg.abs_tilt_mean_rad < TILT_SAMPLE_MEMBER_MIN ||
          agg.abs_tilt_mean_rad > TILT_SAMPLE_MEMBER_MAX) {
        agg.abs_tilt_mean_rad = NAN;
      }
      if (agg.std_tilt_mean_rad < TILT_SAMPLE_MEMBER_MIN ||
          agg.std_tilt_mean_rad > TILT_SAMPLE_MEMBER_MAX) {
        agg.std_tilt_mean_rad = NAN;
      }
    }
    static constexpr uint8_t TIME_STR_BUFSIZE = 50;
    static char timeStrbuf[TIME_STR_BUFSIZE];
    if (!logRtcGetTimeStr(timeStrbuf, TIME_STR_BUFSIZE, true)) {
      printf("Failed to get time string for Aanderaa aggregation\n");
      snprintf(timeStrbuf, TIME_STR_BUFSIZE, "0");
    }

    int8_t node_position = topology_sampler_get_node_position(node_id, pdTICKS_TO_MS(5000));

    log_buflen = snprintf(log_buf, SENSOR_LOG_BUF_SIZE,
                          "%016" PRIx64 "," // Node Id
                          "%" PRIi8 ","     // node_position
                          "aanderaa,"       // node_app_name
                          "%s,"             // timestamp(ticks/UTC)
                          "%" PRIu32 ","    // reading_count
                          "%.3f,"           // abs_speed_mean_cm_s
                          "%.3f,"           // abs_speed_std_cm_s
                          "%.3f,"           // direction_circ_mean_rad
                          "%.3f,"           // direction_circ_std_rad
                          "%.3f,"           // temp_mean_deg_c
                          "%.3f,"           // abs_tilt_mean_rad
                          "%.3f\n",         // std_tilt_mean_rad
                          node_id, node_position, timeStrbuf, agg.reading_count,
                          agg.abs_speed_mean_cm_s, agg.abs_speed_std_cm_s,
                          agg.direction_circ_mean_rad, agg.direction_circ_std_rad,
                          agg.temp_mean_deg_c, agg.abs_tilt_mean_rad, agg.std_tilt_mean_rad);
    if (log_buflen > 0) {
      BRIDGE_SENSOR_LOG_PRINTN(BM_COMMON_AGG, log_buf, log_buflen);
    } else {
      printf("ERROR: Failed to print Aanderaa data\n");
    }
    reportBuilderAddToQueue(node_id, SENSOR_TYPE_AANDERAA, static_cast<void *>(&agg),
                            sizeof(aanderaa_aggregations_t), REPORT_BUILDER_SAMPLE_MESSAGE);
    memset(log_buf, 0, SENSOR_LOG_BUF_SIZE);
    // Clear the buffers
    abs_speed_cm_s.clear();
    direction_rad.clear();
    temp_deg_c.clear();
    abs_tilt_rad.clear();
    std_tilt_rad.clear();
    reading_count = 0;
    xSemaphoreGive(_mutex);
  } else {
    printf("Failed to get the subbed Aanderaa mutex while trying to aggregate\n");
  }
  vPortFree(log_buf);
}

Aanderaa_t *createAanderaaSub(uint64_t node_id, uint32_t current_agg_period_ms,
                              uint32_t averager_max_samples) {
  Aanderaa_t *new_sub = static_cast<Aanderaa_t *>(pvPortMalloc(sizeof(Aanderaa_t)));
  new_sub = new (new_sub) Aanderaa_t();
  configASSERT(new_sub);

  new_sub->_mutex = xSemaphoreCreateMutex();
  configASSERT(new_sub->_mutex);

  new_sub->node_id = node_id;
  new_sub->type = SENSOR_TYPE_AANDERAA;
  new_sub->next = NULL;
  new_sub->current_agg_period_ms = current_agg_period_ms;
  new_sub->abs_speed_cm_s.initBuffer(averager_max_samples);
  new_sub->direction_rad.initBuffer(averager_max_samples);
  new_sub->temp_deg_c.initBuffer(averager_max_samples);
  new_sub->abs_tilt_rad.initBuffer(averager_max_samples);
  new_sub->std_tilt_rad.initBuffer(averager_max_samples);
  new_sub->reading_count = 0;
  return new_sub;
}
