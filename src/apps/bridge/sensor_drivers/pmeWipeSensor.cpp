#include "pmeWipeSensor.h"
#include "FreeRTOS.h"
#include "app_config.h"
#include "avgSampler.h"
#include "bm_config.h"
#include "bm_os.h"
#include "bridgeLog.h"
#include "cbor.h"
#include "device_info.h"
#include "messages/config.h"
#include "pme_wipe_msg.h"
#include "pubsub.h"
#include "reportBuilder.h"
#include "semphr.h"
#include "spotter.h"
#include "stm32_rtc.h"
#include "topology_sampler.h"
#include "util.h"
#include <new>

static PmeWipe_t *CURRENT_SUB;

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
  PmeWipe_t *wipe_sensor =
      static_cast<PmeWipe_t *>(sensorControllerFindSensorById(node_id, SENSOR_TYPE_PME_WIPE));
  if (wipe_sensor && wipe_sensor->type == SENSOR_TYPE_PME_WIPE && wipe_sensor->_mutex) {
    if (bm_semaphore_take(wipe_sensor->_mutex, BM_MAX_DELAY_UINT32) == BmOK) {
      static PmeWipeMsg::Data wipe_data;
      if (PmeWipeMsg::decode(wipe_data, data, data_len) == CborNoError) {
        wipe_sensor->wipe_time_sec.addSample(wipe_data.wipe_time_sec);
        wipe_sensor->start1_mA.addSample(wipe_data.start1_mA);
        wipe_sensor->avg_mA.addSample(wipe_data.avg_mA);
        wipe_sensor->start2_mA.addSample(wipe_data.start2_mA);
        wipe_sensor->final_mA.addSample(wipe_data.final_mA);
        wipe_sensor->rsource.addSample(wipe_data.rsource);

        wipe_sensor->reading_count++;
        BmErr err = wipe_sensor->send_spotter_log_individual(
            "pme_wiper", wipe_data.header, (DEFAULT_PME_WIPER_READING_PERIOD_MS + 1000U),
            "%.3f,%.3f,%.3f,%.3f,%.3f,%.3f\n", wipe_data.wipe_time_sec, wipe_data.start1_mA,
            wipe_data.avg_mA, wipe_data.start2_mA, wipe_data.final_mA, wipe_data.rsource);
        if (err != BmOK) {
          bm_debug("ERROR: Failed to print PME Wiper data to IND log, err: %d\n", err);
        }
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
        .wipe_time_sec_mean = NAN,
        .start1_mA_mean = NAN,
        .avg_mA_mean = NAN,
        .start2_mA_mean = NAN,
        .final_mA_mean = NAN,
        .rsource_mean = NAN,
        .reading_count = 0,
    };

    if (wipe_time_sec.getNumSamples() >= MIN_READINGS_FOR_AGGREGATION) {
      wipe_aggs.wipe_time_sec_mean = wipe_time_sec.getMean();
      wipe_aggs.start1_mA_mean = start1_mA.getMean();
      wipe_aggs.avg_mA_mean = avg_mA.getMean();
      wipe_aggs.start2_mA_mean = start2_mA.getMean();
      wipe_aggs.final_mA_mean = final_mA.getMean();
      wipe_aggs.rsource_mean = rsource.getMean();
      wipe_aggs.reading_count = reading_count;
    }

    BmErr err = send_spotter_log_aggregate(
        "pme_wiper", wipe_aggs.reading_count, "%.3f,%.3f,%.3f,%.3f,%.3f,%.3f\n",
        wipe_aggs.wipe_time_sec_mean, wipe_aggs.start1_mA_mean, wipe_aggs.avg_mA_mean,
        wipe_aggs.start2_mA_mean, wipe_aggs.final_mA_mean, wipe_aggs.rsource_mean);
    if (err != BmOK) {
      bm_debug("ERROR: Failed to print PME Wiper data to AGG log, err: %d\n", err);
    }

    // TODO - figure out if we want to add to the report builder and how we will do it!
    // reportBuilderAddToQueue(node_id, SENSOR_TYPE_PME_WIPE, static_cast<void *>(&wipe_aggs),
    //                         sizeof(pme_wipe_aggregations_t), REPORT_BUILDER_SAMPLE_MESSAGE);

    // Clear the buffers
    memset(log_buff, 0, SENSOR_LOG_BUF_SIZE);
    wipe_time_sec.clear();
    start1_mA.clear();
    avg_mA.clear();
    start2_mA.clear();
    final_mA.clear();
    rsource.clear();
    reading_count = 0;
    bm_semaphore_give(_mutex);
  } else {
    bm_debug("Failed to take mutex for PME Wiper Sensor after getting a new reading\n");
  }
  bm_free(log_buff);
}

static BmErr pmeWiperCfgGetCb(uint8_t *payload) {
  BmErr err = BmENODATA;
  if (payload && CURRENT_SUB) {
    BmConfigValue *msg = reinterpret_cast<BmConfigValue *>(payload);
    size_t size = sizeof(AbstractSensor::m_reading_period_ms);
    err = bcmp_config_decode_value(UINT32, msg->data, msg->data_length,
                                   &CURRENT_SUB->m_reading_period_ms, &size);
    if (err == BmOK) {
      uint32_t averager_max_samples =
          (CURRENT_SUB->m_reading_period_ms / CURRENT_SUB->agg_period_ms) +
          PmeWipeSensor::N_SAMPLES_PAD;

      // We can re-init the buffers here now that we have the reading period
      // This will free the current buffer and re-malloc a new one
      // so any data that may have been in the buffer will be lost
      bridgeLogPrint(BRIDGE_CFG, BM_COMMON_LOG_LEVEL_INFO, USE_HEADER,
                     "Updating the PME Wiper buffers with max samples: %" PRIu32 "\n",
                     averager_max_samples);
      CURRENT_SUB->wipe_time_sec.initBuffer(averager_max_samples);
      CURRENT_SUB->start1_mA.initBuffer(averager_max_samples);
      CURRENT_SUB->avg_mA.initBuffer(averager_max_samples);
      CURRENT_SUB->start2_mA.initBuffer(averager_max_samples);
      CURRENT_SUB->final_mA.initBuffer(averager_max_samples);
      CURRENT_SUB->rsource.initBuffer(averager_max_samples);
    } else {
      bm_debug("Failed to decode PME Wiper config get, err: %d\n", err);
    }
    CURRENT_SUB = NULL;
  }
  return err;
}

PmeWipe_t *createPmeWipeSub(uint64_t node_id, uint32_t sample_duration_ms) {
  PmeWipe_t *new_sub = static_cast<PmeWipe_t *>(bm_malloc(sizeof(PmeWipe_t)));
  if (new_sub) {
    new_sub = new (new_sub) PmeWipe_t();

    new_sub->_mutex = xSemaphoreCreateMutex();

    if (new_sub->_mutex) {
      new_sub->node_id = node_id;
      new_sub->type = SENSOR_TYPE_PME_WIPE;
      new_sub->next = NULL;
      new_sub->agg_period_ms = PmeWipeSensor::DEFAULT_PME_WIPER_READING_PERIOD_MS;

      // This is the default value
      uint32_t averager_max_samples = static_cast<uint32_t>(
          ceil((sample_duration_ms / new_sub->agg_period_ms) + PmeWipeSensor::N_SAMPLES_PAD));

      new_sub->wipe_time_sec.initBuffer(averager_max_samples);
      new_sub->start1_mA.initBuffer(averager_max_samples);
      new_sub->avg_mA.initBuffer(averager_max_samples);
      new_sub->start2_mA.initBuffer(averager_max_samples);
      new_sub->final_mA.initBuffer(averager_max_samples);
      new_sub->rsource.initBuffer(averager_max_samples);
      new_sub->reading_count = 0;

      CURRENT_SUB = new_sub;
      BmErr err = BmOK;
      if (!bcmp_config_get(node_id, BM_CFG_PARTITION_SYSTEM,
                           strlen(AppConfig::PME_WIPER_READING_PERIOD_MS),
                           AppConfig::PME_WIPER_READING_PERIOD_MS, &err, pmeWiperCfgGetCb)) {
        bm_debug("Failed to send PME Wiper config get\n");
      }

    } else {
      bm_free(new_sub);
      new_sub = NULL;
    }
  }
  return new_sub;
}
