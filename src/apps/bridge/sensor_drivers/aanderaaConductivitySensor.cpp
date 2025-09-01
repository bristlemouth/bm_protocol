#include "aanderaaConductivitySensor.h"
#include "app_config.h"
#include "avgSampler.h"
#include "spotter.h"
#include "pubsub.h"
#include "aanderaa_conductivity_msg.h"
#include "bridgeLog.h"
#include "cbor.h"
#include "device_info.h"
#include "reportBuilder.h"
#include "semphr.h"
#include "stm32_rtc.h"
#include "topology_sampler.h"
#include "app_util.h"
#include <new>

#define DEFAULT_AANDERAA_CONDUCTIVITY_READING_PERIOD_MS 1000 // 1 second

bool AanderaaConductivitySensor::subscribe() {
  bool rval = false;
  char *sub = static_cast<char *>(pvPortMalloc(BM_TOPIC_MAX_LEN));
  configASSERT(sub);
  int topic_strlen =
      snprintf(sub, BM_TOPIC_MAX_LEN, "sensor/%016" PRIx64 "%s", node_id, subtag);
  if (topic_strlen > 0) {
    rval = bm_sub_wl(sub, topic_strlen, aanderaaConductivitySubCallback) == BmOK;
  }
  vPortFree(sub);
  return rval;
}

void AanderaaConductivitySensor::aanderaaConductivitySubCallback(uint64_t node_id, const char *topic,
                                                                 uint16_t topic_len,
                                                                 const uint8_t *data,
                                                                 uint16_t data_len, uint8_t type,
                                                                 uint8_t version) {
  (void)type;
  (void)version;
  printf("Aanderaa Conductivity data received from node %016" PRIx64 ", on topic: %.*s\n", node_id,
         topic_len, topic);
  AanderaaConductivity_t *conductivity_sensor =
      static_cast<AanderaaConductivity_t *>(sensorControllerFindSensorById(node_id, SENSOR_TYPE_AANDERAA_CONDUCTIVITY));
  if (conductivity_sensor && conductivity_sensor->type == SENSOR_TYPE_AANDERAA_CONDUCTIVITY) {
    if (xSemaphoreTake(conductivity_sensor->_mutex, portMAX_DELAY)) {
      static AanderaaConductivityMsg::Data conductivity_data;
      if (AanderaaConductivityMsg::decode(conductivity_data, data, data_len) == CborNoError) {
        char *log_buf = static_cast<char *>(pvPortMalloc(SENSOR_LOG_BUF_SIZE));
        configASSERT(log_buf);
        conductivity_sensor->conductivity_ms_cm.addSample(conductivity_data.conductivity_ms_cm);
        conductivity_sensor->temperature_deg_c.addSample(conductivity_data.temperature_deg_c);
        conductivity_sensor->salinity_psu.addSample(conductivity_data.salinity_psu);
        conductivity_sensor->water_density_kg_m3.addSample(conductivity_data.water_density_kg_m3);
        conductivity_sensor->sound_speed_m_s.addSample(conductivity_data.sound_speed_m_s);
        conductivity_sensor->depth_m.addSample(conductivity_data.depth_m);
        conductivity_sensor->reading_count++;

        // Large floats get formatted in scientific notation,
        // so we print integer seconds and millis separately.
        uint64_t reading_time_sec = conductivity_data.header.reading_time_utc_ms / 1000U;
        uint32_t reading_time_millis = conductivity_data.header.reading_time_utc_ms % 1000U;
        uint64_t sensor_reading_time_sec = conductivity_data.header.sensor_reading_time_ms / 1000U;
        uint32_t sensor_reading_time_millis =
            conductivity_data.header.sensor_reading_time_ms % 1000U;

        uint32_t current_timestamp = pdTICKS_TO_MS(xTaskGetTickCount());
        if (current_timestamp - conductivity_sensor->last_timestamp >
                DEFAULT_AANDERAA_CONDUCTIVITY_READING_PERIOD_MS + 1000U ||
            conductivity_sensor->reading_count == 1U) {
          printf("Updating Aanderaa Conductivity %016" PRIx64
                 " node position, current_time = %" PRIu32 ", last_time = %" PRIu32
                 ", reading count: %" PRIu32 "\n",
                 node_id, current_timestamp, conductivity_sensor->last_timestamp,
                 conductivity_sensor->reading_count);
          conductivity_sensor->node_position =
              topology_sampler_get_node_position(node_id, pdTICKS_TO_MS(5000));
        }
        conductivity_sensor->last_timestamp = current_timestamp;

        size_t log_buflen =
            snprintf(log_buf, SENSOR_LOG_BUF_SIZE,
                     "%016" PRIx64 ","        // Node Id
                     "%" PRIi8 ","            // node_position
                     "aanderaa_conductivity," // node_app_name
                     "%" PRIu64 ","           // reading_uptime_millis
                     "%" PRIu64 "."           // reading_time_utc_ms seconds part
                     "%03" PRIu32 ","         // reading_time_utc_ms millis part
                     "%" PRIu64 "."           // sensor_reading_time_ms seconds part
                     "%03" PRIu32 ","         // sensor_reading_time_ms millis part
                     "%.4f,"                  // conductivity_ms_cm
                     "%.3f,"                  // temperature_deg_c
                     "%.3f,"                  // salinity_psu
                     "%.3f,"                  // water_density_kg_m3
                     "%.3f,"                  // sound_speed_m_s
                     "%.3f\n",                // depth_m
                     node_id, conductivity_sensor->node_position,
                     conductivity_data.header.reading_uptime_millis, reading_time_sec,
                     reading_time_millis, sensor_reading_time_sec, sensor_reading_time_millis,
                     conductivity_data.conductivity_ms_cm, conductivity_data.temperature_deg_c,
                     conductivity_data.salinity_psu, conductivity_data.water_density_kg_m3,
                     conductivity_data.sound_speed_m_s, conductivity_data.depth_m);
        if (log_buflen > 0) {
          BRIDGE_SENSOR_LOG_PRINTN(BM_COMMON_IND, log_buf, log_buflen);
        } else {
          printf("ERROR: Failed to print Aanderaa Conductivity data\n");
        }
        vPortFree(log_buf);
      }
      xSemaphoreGive(conductivity_sensor->_mutex);
    }
  }
}

void AanderaaConductivitySensor::aggregate(void) {
  char *log_buf = static_cast<char *>(pvPortMalloc(SENSOR_LOG_BUF_SIZE));
  configASSERT(log_buf);
  if (xSemaphoreTake(_mutex, portMAX_DELAY)) {
    size_t log_buflen = 0;
    aanderaa_conductivity_aggregations_t conductivity_aggs = {
        .conductivity_mean_ms_cm = NAN,
        .temperature_mean_deg_c = NAN,
        .salinity_mean_psu = NAN,
        .water_density_mean_kg_m3 = NAN,
        .sound_speed_mean_m_s = NAN,
        .depth_mean_m = NAN,
        .reading_count = 0};
    if (conductivity_ms_cm.getNumSamples() > MIN_READINGS_FOR_AGGREGATION) {
      conductivity_aggs.conductivity_mean_ms_cm = conductivity_ms_cm.getMean();
      conductivity_aggs.temperature_mean_deg_c = temperature_deg_c.getMean();
      conductivity_aggs.salinity_mean_psu = salinity_psu.getMean();
      conductivity_aggs.water_density_mean_kg_m3 = water_density_kg_m3.getMean();
      conductivity_aggs.sound_speed_mean_m_s = sound_speed_m_s.getMean();
      conductivity_aggs.depth_mean_m = depth_m.getMean();
      conductivity_aggs.reading_count = reading_count;
    }

    reportBuilderAddToQueue(node_id, SENSOR_TYPE_AANDERAA_CONDUCTIVITY, static_cast<void *>(&conductivity_aggs),
                            sizeof(aanderaa_conductivity_aggregations_t), REPORT_BUILDER_SAMPLE_MESSAGE);

    static constexpr uint8_t TIME_STR_BUFSIZE = 50;
    char time_str[TIME_STR_BUFSIZE];
    if (!logRtcGetTimeStr(time_str, TIME_STR_BUFSIZE, true)) {
      printf("Failed to get RTC time string for Aanderaa Conductivity aggregation\n");
      snprintf(time_str, TIME_STR_BUFSIZE, "0");
    }

    int8_t node_position = topology_sampler_get_node_position(node_id, pdTICKS_TO_MS(5000));

    log_buflen =
        snprintf(log_buf, SENSOR_LOG_BUF_SIZE,
                 "%016" PRIx64 ","        // Node Id
                 "%" PRIi8 ","            // node_position
                 "aanderaa_conductivity," // node_app_name
                 "%s,"                    // timestamp(ticks/UTC)
                 "%" PRIu32 ","           // reading_count
                 "%.4f,"                  // conductivity_mean_ms_cm
                 "%.3f,"                  // temperature_mean_deg_c
                 "%.3f,"                  // salinity_mean_psu
                 "%.3f,"                  // water_density_mean_kg_m3
                 "%.3f,"                  // sound_speed_mean_m_s
                 "%.3f\n",                // depth_mean_m
                 node_id, node_position, time_str, conductivity_aggs.reading_count,
                 conductivity_aggs.conductivity_mean_ms_cm, conductivity_aggs.temperature_mean_deg_c,
                 conductivity_aggs.salinity_mean_psu, conductivity_aggs.water_density_mean_kg_m3,
                 conductivity_aggs.sound_speed_mean_m_s, conductivity_aggs.depth_mean_m);
    if (log_buflen > 0) {
      BRIDGE_SENSOR_LOG_PRINTN(BM_COMMON_AGG, log_buf, log_buflen);
    } else {
      printf("ERROR: Failed to print Aanderaa Conductivity aggregation to log\n");
    }

    // Clear the buffers
    conductivity_ms_cm.clear();
    temperature_deg_c.clear();
    salinity_psu.clear();
    water_density_kg_m3.clear();
    sound_speed_m_s.clear();
    depth_m.clear();
    reading_count = 0;
    xSemaphoreGive(_mutex);
  } else {
    printf("Failed to get the Aanderaa Conductivity mutex while trying to aggregate\n");
  }
  vPortFree(log_buf);
}

AanderaaConductivity_t *createAanderaaConductivitySub(uint64_t node_id, uint32_t agg_period_ms,
                                                      uint32_t averager_max_samples) {
  AanderaaConductivity_t *new_sub =
      static_cast<AanderaaConductivity_t *>(pvPortMalloc(sizeof(AanderaaConductivity_t)));
  new_sub = new (new_sub) AanderaaConductivity_t();
  configASSERT(new_sub);

  new_sub->_mutex = xSemaphoreCreateMutex();
  configASSERT(new_sub->_mutex);

  new_sub->node_id = node_id;
  new_sub->type = SENSOR_TYPE_AANDERAA_CONDUCTIVITY;
  new_sub->next = NULL;
  new_sub->current_agg_period_ms = agg_period_ms;
  new_sub->conductivity_ms_cm.initBuffer(averager_max_samples);
  new_sub->temperature_deg_c.initBuffer(averager_max_samples);
  new_sub->salinity_psu.initBuffer(averager_max_samples);
  new_sub->water_density_kg_m3.initBuffer(averager_max_samples);
  new_sub->sound_speed_m_s.initBuffer(averager_max_samples);
  new_sub->depth_m.initBuffer(averager_max_samples);
  new_sub->reading_count = 0;
  return new_sub;
}