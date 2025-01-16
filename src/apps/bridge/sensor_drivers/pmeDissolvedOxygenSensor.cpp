#include "pmeDissolvedOxygenSensor.h"
#include "FreeRTOS.h"
#include "app_config.h"
#include "avgSampler.h"
#include "bm_config.h"
#include "bm_os.h"
#include "bridgeLog.h"
#include "cbor.h"
#include "device_info.h"
#include "messages/config.h"
#include "pme_dissolved_oxygen_msg.h"
#include "pubsub.h"
#include "reportBuilder.h"
#include "semphr.h"
#include "spotter.h"
#include "stm32_rtc.h"
#include "topology_sampler.h"
#include "util.h"
#include <new>

static PmeDissolvedOxygen_t *CURRENT_SUB;

bool PmeDissolvedOxygenSensor::subscribe() {
  bool rval = false;
  char *sub = static_cast<char *>(bm_malloc(BM_TOPIC_MAX_LEN));
  if (sub) {
    int topic_strlen =
        snprintf(sub, BM_TOPIC_MAX_LEN, "sensor/%016" PRIx64 "%s", node_id, subtag);
    if (topic_strlen > 0) {
      rval = (bm_sub_wl(sub, topic_strlen, pmeDissolvedOxygenSubCallback) == BmOK);
    }
    bm_free(sub);
  }
  return rval;
}

void PmeDissolvedOxygenSensor::pmeDissolvedOxygenSubCallback(
    uint64_t node_id, const char *topic, uint16_t topic_len, const uint8_t *data,
    uint16_t data_len, uint8_t type, uint8_t version) {
  (void)type;
  (void)version;
  bm_debug("PME Dissolved Oxygen data received from node %016" PRIx64 ", on topic: %.*s\n",
           node_id, topic_len, topic);
  PmeDissolvedOxygen_t *dissolved_oxygen_sensor = static_cast<PmeDissolvedOxygen_t *>(
      sensorControllerFindSensorById(node_id, SENSOR_TYPE_PME_DO));
  if (dissolved_oxygen_sensor && dissolved_oxygen_sensor->type == SENSOR_TYPE_PME_DO &&
      dissolved_oxygen_sensor->_mutex) {
    if (bm_semaphore_take(dissolved_oxygen_sensor->_mutex, BM_MAX_DELAY_UINT32) == BmOK) {
      static PmeDissolvedOxygenMsg::Data dissolved_oxygen_data;
      if (PmeDissolvedOxygenMsg::decode(dissolved_oxygen_data, data, data_len) == CborNoError) {
        char *log_buff = static_cast<char *>(bm_malloc(SENSOR_LOG_BUF_SIZE));
        configASSERT(log_buff);
        dissolved_oxygen_sensor->temperature_deg_c.addSample(
            dissolved_oxygen_data.temperature_deg_c);
        dissolved_oxygen_sensor->do_mg_per_l.addSample(dissolved_oxygen_data.do_mg_per_l);
        dissolved_oxygen_sensor->quality.addSample(dissolved_oxygen_data.quality);
        dissolved_oxygen_sensor->do_saturation_pct.addSample(
            dissolved_oxygen_data.do_saturation_pct);
        dissolved_oxygen_sensor->reading_count++;

        BmErr err = dissolved_oxygen_sensor->send_spotter_log_individual(
            "pme_dissolved_oxygen", dissolved_oxygen_data.header,
            (DEFAULT_PME_DISSOLVED_READING_PERIOD_MS + 1000U),
            "%.4f,"   // temperature_deg_c
            "%.3f,"   // do_mg_per_l
            "%.3f,"   // quality
            "%.1f\n", // do_saturation_pct
            dissolved_oxygen_data.temperature_deg_c, dissolved_oxygen_data.do_mg_per_l,
            dissolved_oxygen_data.quality, dissolved_oxygen_data.do_saturation_pct);
        if (err != BmOK) {
          bm_debug("ERROR: Failed to print PME Dissolved Oxygen data to IND log, err: %d\n",
                   err);
        }
      }
      bm_semaphore_give(dissolved_oxygen_sensor->_mutex);
    } else {
      bm_debug(
          "Failed to take mutex for PME Dissolved Oxygen Sensor after getting a new reading\n");
    }
  }
}

void PmeDissolvedOxygenSensor::aggregate(void) {
  char *log_buff = static_cast<char *>(bm_malloc(SENSOR_LOG_BUF_SIZE));
  configASSERT(log_buff);
  if (bm_semaphore_take(_mutex, BM_MAX_DELAY_UINT32) == BmOK) {
    pme_dissolved_oxygen_aggregations_t dissolved_oxygen_aggs = {.temperature_deg_c_mean = NAN,
                                                                 .do_mg_per_l_mean = NAN,
                                                                 .quality_mean = NAN,
                                                                 .do_saturation_pct_mean = NAN,
                                                                 .reading_count = 0};

    if (temperature_deg_c.getNumSamples() >= MIN_READINGS_FOR_AGGREGATION) {
      dissolved_oxygen_aggs.temperature_deg_c_mean = temperature_deg_c.getMean();
      dissolved_oxygen_aggs.do_mg_per_l_mean = do_mg_per_l.getMean();
      dissolved_oxygen_aggs.quality_mean = quality.getMean();
      dissolved_oxygen_aggs.do_saturation_pct_mean = do_saturation_pct.getMean();
      dissolved_oxygen_aggs.reading_count = reading_count;
    }

    BmErr err = send_spotter_log_aggregate(
        "pme_dissolved_oxygen", dissolved_oxygen_aggs.reading_count,
        "%.4f,"   // temperature_deg_c
        "%.3f,"   // do_mg_per_l
        "%.3f,"   // quality
        "%.1f\n", // do_saturation_pct
        dissolved_oxygen_aggs.temperature_deg_c_mean, dissolved_oxygen_aggs.do_mg_per_l_mean,
        dissolved_oxygen_aggs.quality_mean, dissolved_oxygen_aggs.do_saturation_pct_mean);
    if (err != BmOK) {
      bm_debug("ERROR: Failed to print PME Dissolved Oxygen data to AGG log, err: %d\n", err);
    }

    reportBuilderAddToQueue(
        node_id, SENSOR_TYPE_PME_DO, static_cast<void *>(&dissolved_oxygen_aggs),
        sizeof(pme_dissolved_oxygen_aggregations_t), REPORT_BUILDER_SAMPLE_MESSAGE);

    // Clear the buffers
    memset(log_buff, 0, SENSOR_LOG_BUF_SIZE);
    temperature_deg_c.clear();
    do_mg_per_l.clear();
    quality.clear();
    do_saturation_pct.clear();
    reading_count = 0;
    bm_semaphore_give(_mutex);
  } else {
    bm_debug(
        "Failed to take mutex for PME Dissolved Oxygen Sensor after getting a new reading\n");
  }
  bm_free(log_buff);
}

static BmErr pmeDissolvedOxygenCfgGetCb(uint8_t *payload) {
  BmErr err = BmENODATA;

  if (payload && CURRENT_SUB) {
    BmConfigValue *msg = reinterpret_cast<BmConfigValue *>(payload);
    size_t size = sizeof(AbstractSensor::m_reading_period_ms);
    err = bcmp_config_decode_value(UINT32, msg->data, msg->data_length,
                                   &CURRENT_SUB->m_reading_period_ms, &size);
    if (err == BmOK) {
      uint32_t averager_max_samples =
          (CURRENT_SUB->m_reading_period_ms / CURRENT_SUB->agg_period_ms) +
          PmeDissolvedOxygen_t::N_SAMPLES_PAD;

      bridgeLogPrint(BRIDGE_CFG, BM_COMMON_LOG_LEVEL_INFO, USE_HEADER,
                     "Updating the PME Dissolved Oxygen buffers with max samples  %" PRIu32
                     "\n", averager_max_samples);
      CURRENT_SUB->temperature_deg_c.initBuffer(averager_max_samples);
      CURRENT_SUB->do_mg_per_l.initBuffer(averager_max_samples);
      CURRENT_SUB->quality.initBuffer(averager_max_samples);
      CURRENT_SUB->do_saturation_pct.initBuffer(averager_max_samples);
    } else {
      bm_debug("Failed to decode PME Dissolved Oxygen config get, err: %d\n", err);
    }
    CURRENT_SUB = NULL;
  }

  return err;
}

PmeDissolvedOxygen_t *createPmeDissolvedOxygenSub(uint64_t node_id, uint32_t sample_duration_ms) {
  PmeDissolvedOxygen_t *new_sub =
      static_cast<PmeDissolvedOxygen_t *>(bm_malloc(sizeof(PmeDissolvedOxygen_t)));
  if (new_sub) {
    new_sub = new (new_sub) PmeDissolvedOxygen_t();

    new_sub->_mutex = xSemaphoreCreateMutex();

    if (new_sub->_mutex) {
      new_sub->node_id = node_id;
      new_sub->type = SENSOR_TYPE_PME_DO;
      new_sub->next = NULL;
      new_sub->agg_period_ms =
          PmeDissolvedOxygenSensor::DEFAULT_PME_DISSOLVED_READING_PERIOD_MS;

      uint32_t averager_max_samples =
          static_cast<uint32_t>(ceil((sample_duration_ms / new_sub->agg_period_ms) +
                                     PmeDissolvedOxygenSensor::N_SAMPLES_PAD));

      new_sub->temperature_deg_c.initBuffer(averager_max_samples);
      new_sub->do_mg_per_l.initBuffer(averager_max_samples);
      new_sub->quality.initBuffer(averager_max_samples);
      new_sub->do_saturation_pct.initBuffer(averager_max_samples);
      new_sub->reading_count = 0;

      CURRENT_SUB = new_sub;
      BmErr err = BmOK;
      if (!bcmp_config_get(node_id, BM_CFG_PARTITION_SYSTEM,
                           strlen(AppConfig::PME_DISSOLVED_OXYGEN_PERIOD_MS),
                           AppConfig::PME_DISSOLVED_OXYGEN_PERIOD_MS, &err,
                           pmeDissolvedOxygenCfgGetCb)) {
        bm_debug("Failed to send PME Wiper config get\n");
      }

    } else {
      bm_free(new_sub);
      new_sub = NULL;
    }
  }
  return new_sub;
}