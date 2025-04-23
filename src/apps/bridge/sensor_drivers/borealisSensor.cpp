#include "borealisSensor.h"
#include "FreeRTOS.h"
#include "app_config.h"
#include "bm_config.h"
#include "bm_os.h"
#include "bridgeLog.h"
#include "bristlemouth.h"
#include "messages/config.h"
#include "pubsub.h"
#include "semphr.h"
#include "sensorController.h"
#include <inttypes.h>
#include <new>
#include <string.h>

#define MAX_BOREALIS_READING_PERIOD_MS(period) (period + 1000U)

static constexpr char READING_PERIOD_KEY[] = "bandsSampleTimeMs";
static struct BorealisSensor *CURRENT_SUB;

/*!
 @brief Subscribe To Borealis Topic

 @details Subscribes to a specific nodes borealis topic

 @return true on success
 @return false on failure
 */
bool BorealisSensor::subscribe() {
  bool ret = false;
  char *sub = static_cast<char *>(bm_malloc(BM_TOPIC_MAX_LEN));
  const char *subtag = NULL;

  switch (type) {
  case SENSOR_TYPE_BOREALIS_SPECTRUM:
    subtag = subtag_spectrum;
    break;
  case SENSOR_TYPE_BOREALIS_LEVELS:
    subtag = subtag_levels;
    break;
  case SENSOR_TYPE_BOREALIS_LEVEL_STATISTICS:
    subtag = subtag_level_statistics;
    break;
  case SENSOR_TYPE_BOREALIS_RECORDING_STATUS:
    subtag = subtag_recstatus;
    break;
  default:
    break;
  }

  if (sub && subtag) {
    uint32_t topic_strlen =
        snprintf(sub, BM_TOPIC_MAX_LEN, "sensor/%016" PRIx64 "%s", node_id, subtag);
    if (topic_strlen > 0) {
      ret = bm_sub_wl(sub, topic_strlen, borealisSubCallback) == BmOK;
    }
    bm_free(sub);
  }
  return ret;
}

/*!
 @brief Aggregate Data To Send To Report Builder

 @details This takes the latest level statistics messages and sends it to the SENS_AGG
          file on spotter, calculates SPL and entropy, as well as adds the message
          LevelStatisticsData_t to the report builder.
 */
void BorealisSensorLevelStatistics::aggregate(void) {
  uint32_t count = 0;
  BmErr err = BmOK;
  LevelStatisticsData_t *data = NULL;
  uint32_t data_size = sizeof(LevelStatisticsData_t) + stats.levels_length;

  if (stats.dt != 0) {
    data = reinterpret_cast<LevelStatisticsData_t *>(bm_malloc(data_size));
    if (data) {
      memset(data, 0, data_size);
      count = (uint32_t)(stats.dt_report / stats.dt);
      err = send_spotter_log_aggregate("borealis", count, "%.3f,%.3f,%.3f,%u,%.*s\n", stats.dt,
                                       stats.dt_report, stats.max_iqr, stats.first_band_index,
                                       stats.levels_length, stats.levels);

      if (err != BmOK) {
        bm_debug("Failed to send borealis aggregated log to spotter, reason: %d\n", err);
      }

      // TODO: logic needs to be implemented around this
      data->is_extended = 1;

      // TODO: these need calculations or updates
      data->spl = 0;
      data->maq_iqr_band = 0;
      data->entropy = 0;

      data->maq_iqr = stats.max_iqr;

      if (data->is_extended) {
        data->spl_band_stats_size = stats.levels_length;
        memcpy(data->spl_bands_stats, stats.levels, stats.levels_length);
      }

      reportBuilderAddToQueue(node_id, SENSOR_TYPE_BOREALIS_LEVEL_STATISTICS, data,
                              sizeof(LevelStatisticsData_t) + stats.levels_length,
                              REPORT_BUILDER_SAMPLE_MESSAGE);
      bm_free(data);
    } else {
      bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_ERROR, USE_HEADER,
                     "Failed to allocate memory for Borealis level statistics memory in %s\n",
                     __func__);
    }
  }

  // Free stats levels message
  bm_free(stats.levels);
  stats = (struct borealis_level_statistics){};
}

/*!
 @brief Encode A Levels Statistics Sample

 @details Used by report builder to CBOR encode an aggregated sample of Borealis data.

 @param data data to be encoded
 @param sample_index index of data to be accessed
 @param context sensor report context for the data to be encoded to

 @return BmOK on success
 @return BmErr on failure
 */
BmErr BorealisSensorLevelStatistics::encode_sample(void *data, uint32_t sample_index,
                                                   sensor_report_encoder_context_t &context) {
  BmErr err = BmOK;
  LevelStatisticsData_t borealis_data =
      static_cast<LevelStatisticsData_t *>(data)[sample_index];
  uint8_t num_fields = NUM_AOS_BOREALIS_FIELDS;

  if (borealis_data.is_extended) {
    num_fields = NUM_AOS_BOREALIS_FIELDS_EXTENDED;
  }

  if (sensor_report_encoder_open_sample(context, num_fields, "bm_aos_borealis_v0") !=
      CborNoError) {
    bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_ERROR, USE_HEADER,
                   "Failed to open borealis sample in %s\n", __func__);
    err = BmEBADMSG;
  }

  if (err == BmOK &&
      (sensor_report_encoder_add_sample_member(context, encode_uint8_sample_member,
                                               &borealis_data.is_extended) != CborNoError ||
       sensor_report_encoder_add_sample_member(context, encode_uint8_sample_member,
                                               &borealis_data.spl) != CborNoError ||
       sensor_report_encoder_add_sample_member(context, encode_uint8_sample_member,
                                               &borealis_data.maq_iqr) != CborNoError ||
       sensor_report_encoder_add_sample_member(context, encode_uint8_sample_member,
                                               &borealis_data.maq_iqr_band) != CborNoError ||
       sensor_report_encoder_add_sample_member(context, encode_uint8_sample_member,
                                               &borealis_data.entropy) != CborNoError ||
       (borealis_data.is_extended &&
        sensor_report_encoder_add_sample_member(
            context, encode_string_sample_member, borealis_data.spl_bands_stats,
            borealis_data.spl_band_stats_size) != CborNoError))) {
    bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_ERROR, USE_HEADER,
                   "Failed to add borealis level statistics sample member in %s\n", __func__);
    err = BmEBADMSG;
  }

  if (err == BmOK && sensor_report_encoder_close_sample(context) != CborNoError) {
    bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_ERROR, USE_HEADER,
                   "Failed to close sample in %s\n", __func__);
    err = BmEBADMSG;
  }

  return err;
}

/*!
 @brief Subscription Callback For Borealis Topic

 @details This function decodes the subscribed borealis message and sends
          formmated data to spotter as an individual reading.

 @param node_id node ID from subscription
 @param topic topic from subscription
 @param topic_len length of topic
 @param data data from subscribed message
 @param data_len length of data
 @param type unused
 @param version unused
 */
void BorealisSensor::borealisSubCallback(uint64_t node_id, const char *topic,
                                         uint16_t topic_len, const uint8_t *data,
                                         uint16_t data_len, uint8_t type, uint8_t version) {
  (void)type;
  (void)version;
  BmErr err = BmEINVAL;
  Borealis_t *borealis = NULL;
  abstractSensorType_e sensor_type = SENSOR_TYPE_UNKNOWN;

  if (strstr(topic, subtag_spectrum) != NULL) {
    sensor_type = SENSOR_TYPE_BOREALIS_SPECTRUM;
  } else if (strstr(topic, subtag_levels) != NULL) {
    sensor_type = SENSOR_TYPE_BOREALIS_LEVELS;
  } else if (strstr(topic, subtag_level_statistics) != NULL) {
    sensor_type = SENSOR_TYPE_BOREALIS_LEVEL_STATISTICS;
  } else if (strstr(topic, subtag_recstatus) != NULL) {
    sensor_type = SENSOR_TYPE_BOREALIS_RECORDING_STATUS;
  }

  if (sensor_type != SENSOR_TYPE_UNKNOWN) {
    borealis = static_cast<Borealis_t *>(sensorControllerFindSensorById(node_id, sensor_type));
  }

  bm_debug("Borealis data received from node %016" PRIx64 ", on topic: %.*s\n", node_id,
           topic_len, topic);
  if (borealis && borealis->_mutex) {
    if (xSemaphoreTake(borealis->_mutex, portMAX_DELAY) == pdTRUE) {
      switch (borealis->type) {
      case SENSOR_TYPE_BOREALIS_SPECTRUM: {
        struct borealis_spectrum_data d;
        if (borealis_spectrum_data_decode(&d, (uint8_t *)data, data_len) == CborNoError) {
          err = borealis->send_spotter_log_individual(
              "borealis", d.header,
              MAX_BOREALIS_READING_PERIOD_MS(borealis->m_reading_period_ms),
              "%.3f,%.3f,%u,%.*s\n", d.dt, d.df, d.bands_per_octave, d.spectrum_length,
              d.spectrum_as_base64);
          bm_free(d.spectrum_as_base64);
        }
      } break;
      case SENSOR_TYPE_BOREALIS_LEVELS: {
        struct borealis_levels d;
        if (borealis_levels_decode(&d, (uint8_t *)data, data_len) == CborNoError) {
          err = borealis->send_spotter_log_individual(
              "borealis", d.header,
              MAX_BOREALIS_READING_PERIOD_MS(borealis->m_reading_period_ms), "%.3f,%u,%.*s\n",
              d.dt, d.first_band_index, d.levels_length, d.levels);
          bm_free(d.levels);
        }
      } break;
      case SENSOR_TYPE_BOREALIS_LEVEL_STATISTICS: {
        struct borealis_level_statistics d;
        if (borealis_levels_statistics_decode(&d, (uint8_t *)data, data_len) == CborNoError) {
          BorealisLevelsStatistics_t *level_statistics =
              reinterpret_cast<BorealisLevelsStatistics_t *>(borealis);
          power_config_s power_cfg = getPowerConfigs();

          level_statistics->stats = d;

          if (!power_cfg.bridgePowerControllerEnabled) {
            // Free levels information here as it will not be aggregated
            bm_free(d.levels);
          }
          err = BmOK;
        } else {
          err = BmEBADMSG;
        }
      } break;
      case SENSOR_TYPE_BOREALIS_RECORDING_STATUS: {
        struct borealis_recording_status d;
        if (borealis_recording_status_decode(&d, (uint8_t *)data, data_len) == CborNoError) {
          err = borealis->send_spotter_log_individual(
              "borealis", d.header,
              MAX_BOREALIS_READING_PERIOD_MS(borealis->m_reading_period_ms),
              "%.u,%.3f,%.3f,%.*s\n", d.flags, d.seconds_written, d.seconds_free,
              d.filename_length, d.filename);
          bm_free(d.filename);
        }
      } break;
      default:
        break;
      }
      if (err != BmOK) {
        bm_debug("Failed to send borealis individual log to spotter, reason: %d\n", err);
      }
      xSemaphoreGive(borealis->_mutex);
    } else {
      bm_debug("Failed to take the subbed Borealis mutex after getting a new reading\n");
    }
  }
}

static BmErr borealisConfigCb(uint8_t *payload) {
  BmErr err = BmENODATA;

  if (payload && CURRENT_SUB) {
    BmConfigValue *msg = reinterpret_cast<BmConfigValue *>(payload);
    size_t size = sizeof(AbstractSensor::m_reading_period_ms);
    err = bcmp_config_decode_value(UINT32, msg->data, msg->data_length,
                                   &CURRENT_SUB->m_reading_period_ms, &size);
    CURRENT_SUB = NULL;
  }

  return err;
}

/*!
 @brief Creates A Borealis Sensor Subscriber

 @param node_id node ID of the subscriber
 @param reading_period_ms reading period of

 @return pointer to new borealis subscriber
 @return nullptr on failure
 */
Borealis_t *createBorealisSensorSub(abstractSensorType type, uint64_t node_id) {
  Borealis_t *ret = nullptr;
  BmErr err = BmOK;

  if (type == SENSOR_TYPE_BOREALIS_LEVEL_STATISTICS) {
    Borealis_t *sub = static_cast<Borealis_t *>(bm_malloc(sizeof(BorealisLevelsStatistics_t)));
    if (sub) {
      ret = new (sub) BorealisLevelsStatistics_t();
    }
  } else {
    Borealis_t *sub = static_cast<Borealis_t *>(bm_malloc(sizeof(Borealis_t)));
    if (sub) {
      ret = new (sub) Borealis_t();
    }
  }

  if (ret) {

    ret->_mutex = xSemaphoreCreateMutex();

    if (ret->_mutex) {
      ret->node_id = node_id;
      ret->type = type;
      ret->next = nullptr;
      ret->m_reading_period_ms = BorealisSensor::DEFAULT_BOREALIS_READING_PERIOD_MS;
      CURRENT_SUB = ret;
      bcmp_config_get(node_id, BM_CFG_PARTITION_SYSTEM, strlen(READING_PERIOD_KEY),
                      READING_PERIOD_KEY, &err, borealisConfigCb);
    }

    if (!ret->_mutex || err != BmOK) {
      bm_debug("Failed to create borealis sensor err: %d\n", err);
      bm_free(ret);
      ret = nullptr;
    }
  } else {
    bm_debug("Failed to allocate memory for borealis sensor\n");
  }

  return ret;
}
