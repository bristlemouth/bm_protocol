#include "borealisSensor.h"
#include "app_config.h"
#include "bm_config.h"
#include "bm_os.h"
#include "bridgeLog.h"
#include "bristlemouth.h"
#include "mbedtls_base64/base64.h"
#include "messages/config.h"
#include "pubsub.h"
#include "sensorController.h"
#include <inttypes.h>
#include <math.h>
#include <new>
#include <string.h>

#define MAX_BOREALIS_READING_PERIOD_MS(period) (period + 1000U)

static struct BorealisSensor *s_current_sub = NULL;
static SemaphoreHandle_t s_config_callback_handshake = NULL;
static constexpr char subtag_spectrum[] = "/aos/borealis/spectrum";
static constexpr char subtag_levels[] = "/aos/borealis/levels";
static constexpr char subtag_level_statistics[] = "/aos/borealis/level_statistics";
static constexpr char subtag_recstatus[] = "/aos/borealis/recstatus";

/*!
 @brief Initialize Borealis Sensor Module

 @details This will only run once to setup the borealis units on the network
 */
void BorealisSensor::init(void) {
  static constexpr uint32_t MAX_SAMPLES_PER_REPORT = 2;
  uint32_t samples_per_report = 0;
  power_config_s pwr_cfg = {};

  m_aggregation_reports = report_builder_get_transmit_aggregations();
  samples_per_report = report_builder_get_samples_per_report();

  if (samples_per_report > MAX_SAMPLES_PER_REPORT) {
    bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_WARNING, USE_HEADER,
                   "Number of configured samplesPerReport higher than recommended for BOREALIS "
                   "system, there may be issues upon reporting data remotely: "
                   "Configured - %d, Recommended - %d\n",
                   samples_per_report, MAX_SAMPLES_PER_REPORT);
  }

  if (pwr_cfg.subsampleEnabled) {
    bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_WARNING, USE_HEADER,
                   "Subsampling enabled, be aware that BOREALIS data will only transmit last "
                   "collected level statistics message in sampleDurationsMs period remotely\n");
  }
}

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

  if (sub) {
    for (uint32_t i = 0; i < BOREALIS_SUB_COUNT; i++) {
      switch (i) {
      case BOREALIS_SPECTRUM:
        subtag = subtag_spectrum;
        break;
      case BOREALIS_LEVELS:
        subtag = subtag_levels;
        break;
      case BOREALIS_LEVEL_STATISTICS:
        subtag = subtag_level_statistics;
        break;
      case BOREALIS_RECORDING_STATUS:
        subtag = subtag_recstatus;
        break;
      default:
        subtag = NULL;
        break;
      }

      if (subtag) {
        uint32_t topic_strlen =
            snprintf(sub, BM_TOPIC_MAX_LEN, "sensor/%016" PRIx64 "%s", node_id, subtag);
        if (topic_strlen > 0) {
          ret = bm_sub_wl(sub, topic_strlen, borealisSubCallback) == BmOK;
        }
      }
    }

    bm_free(sub);
  }

  return ret;
}

BmErr BorealisSensor::calculateQuantizedSpl(uint8_t const *const band_stats,
                                            const size_t band_stats_len, uint8_t &spl,
                                            uint8_t &max_iqr_band) {
  if (band_stats == NULL || band_stats_len == 0) {
    spl = REPORT_NAN_ERROR_VALUE;
    max_iqr_band = REPORT_NAN_ERROR_VALUE;
    return BmEINVAL;
  }

  const float db_step = 0.75f;
  const float db_min = -256.0f * db_step;
  const unsigned int num_stats = 4; // 25%, 50%, 75%, mean
  const unsigned int num_bands = band_stats_len / num_stats;

  uint8_t max_iqr_so_far = 0;
  float spl_linear_sum = 0.0f;
  for (size_t band_index = 0; band_index < num_bands; band_index++) {
    size_t offset = band_index * num_stats;

    // Search for the band with the maximum interquartile range (IQR)
    uint8_t q25 = band_stats[offset + 0];
    uint8_t q75 = band_stats[offset + 2];
    uint8_t iqr = q75 - q25;
    if (iqr > max_iqr_so_far) {
      max_iqr_so_far = iqr;
      max_iqr_band = band_index;
    }

    // Sum mean SPL for all bands in linear units
    float mean_db = band_stats[offset + 3] * db_step + db_min;
    spl_linear_sum += powf(10.0f, mean_db / 10.0f);
  }

  float broadband_spl_db = 10.0f * log10f(spl_linear_sum);
  spl = fmaxf(0.0f, fminf(255.0f, broadband_spl_db - db_min) / db_step) + 0.5f;

  return BmOK;
}

/*!
 @brief Aggregate Data To Send To Report Builder

 @details This takes the latest level statistics messages calculates SPL and entropy,
          as well as adds the message BorealisLevelStatisticsData_t to the report builder.
 */
void BorealisSensor::aggregate(void) {
  if (xSemaphoreTake(_mutex, portMAX_DELAY) == pdTRUE) {
    // TODO: logic needs to be implemented around if this is going to be extended
    bool is_extended = true;
    size_t decoded_byte_len = 0;
    BorealisLevelStatisticsData_t report = {};
    report.max_iqr = stats.max_iqr;

    mbedtls_base64_decode(NULL, 0, &decoded_byte_len, (const unsigned char *)stats.levels,
                          stats.levels_length);

    report.spl_band_stats = static_cast<uint8_t *>(bm_malloc(decoded_byte_len));
    if (report.spl_band_stats) {
      report.is_extended = is_extended;

      int err = mbedtls_base64_decode(report.spl_band_stats, decoded_byte_len,
                                      (size_t *)&report.spl_band_stats_size,
                                      (const unsigned char *)stats.levels, stats.levels_length);
      if (err != 0) {
        bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_ERROR, USE_HEADER,
                       "Failed to base64 decode Borealis level statistics. "
                       "Probably invalid character. err=%d, len=%lu, b64=%.*s\n",
                       err, stats.levels_length, stats.levels_length, stats.levels);
      } else {
        calculateQuantizedSpl(report.spl_band_stats, report.spl_band_stats_size, report.spl,
                              report.max_iqr_band);
        report.max_iqr_band += stats.first_band_index;
      }
    }

    // TODO: calculate entropy
    report.entropy = REPORT_NAN_ERROR_VALUE;

    reportBuilderAddToQueue(node_id, SENSOR_TYPE_BOREALIS, &report,
                            sizeof(BorealisLevelStatisticsData_t),
                            REPORT_BUILDER_SAMPLE_MESSAGE);
  } else {
    bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_ERROR, USE_HEADER,
                   "Failed to allocate memory for Borealis level statistics in %s\n", __func__);
  }
  xSemaphoreGive(_mutex);

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
BmErr BorealisSensor::encode_sample(void *data, uint32_t sample_index,
                                    sensor_report_encoder_context_t &context) {
  static constexpr uint8_t NUM_AOS_BOREALIS_FIELDS = 5;
  static constexpr uint8_t NUM_AOS_BOREALIS_FIELDS_EXTENDED = 6;
  BmErr err = BmOK;
  BorealisLevelStatisticsData_t borealis_data =
      (static_cast<BorealisLevelStatisticsData_t *>(data))[sample_index];
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
      (sensor_report_encoder_add_sample_member(context, encode_uint_sample_member,
                                               &borealis_data.is_extended) != CborNoError ||
       sensor_report_encoder_add_sample_member(context, encode_uint_sample_member,
                                               &borealis_data.spl) != CborNoError ||
       sensor_report_encoder_add_sample_member(context, encode_uint_sample_member,
                                               &borealis_data.max_iqr) != CborNoError ||
       sensor_report_encoder_add_sample_member(context, encode_uint_sample_member,
                                               &borealis_data.max_iqr_band) != CborNoError ||
       sensor_report_encoder_add_sample_member(context, encode_uint_sample_member,
                                               &borealis_data.entropy) != CborNoError ||
       (borealis_data.is_extended &&
        sensor_report_encoder_add_sample_member(
            context, encode_buffer_sample_member, borealis_data.spl_band_stats,
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

  // Free band stats
  bm_free(borealis_data.spl_band_stats);

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
  BorealisSubscriptions_t sub_type = BOREALIS_SUB_COUNT;

  if (strstr(topic, subtag_spectrum) != NULL) {
    sub_type = BOREALIS_SPECTRUM;
  } else if (strstr(topic, subtag_levels) != NULL) {
    sub_type = BOREALIS_LEVELS;
  } else if (strstr(topic, subtag_level_statistics) != NULL) {
    sub_type = BOREALIS_LEVEL_STATISTICS;
  } else if (strstr(topic, subtag_recstatus) != NULL) {
    sub_type = BOREALIS_RECORDING_STATUS;
  }

  if (sub_type < BOREALIS_SUB_COUNT) {
    borealis = static_cast<Borealis_t *>(
        sensorControllerFindSensorById(node_id, SENSOR_TYPE_BOREALIS));
  }

  bm_debug("Borealis data received from node %016" PRIx64 ", on topic: %.*s\n", node_id,
           topic_len, topic);
  if (borealis && borealis->_mutex) {
    if (xSemaphoreTake(borealis->_mutex, portMAX_DELAY) == pdTRUE) {
      switch (sub_type) {
      case BOREALIS_SPECTRUM: {
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
      case BOREALIS_LEVELS: {
        struct borealis_levels d;
        if (borealis_levels_decode(&d, (uint8_t *)data, data_len) == CborNoError) {
          err = borealis->send_spotter_log_individual(
              "borealis", d.header,
              MAX_BOREALIS_READING_PERIOD_MS(borealis->m_reading_period_ms), "%.3f,%u,%.*s\n",
              d.dt, d.first_band_index, d.levels_length, d.levels);
          bm_free(d.levels);
        }
      } break;
      case BOREALIS_LEVEL_STATISTICS: {
        struct borealis_level_statistics d;
        if (borealis_levels_statistics_decode(&d, (uint8_t *)data, data_len) == CborNoError) {
          uint32_t count = 0;

          if (borealis->stats.levels) {
            // This would be an indication that the user has configured BOREALIS' report_interval
            // to be less than half of the bridge power controller's sampleDurationMs, we cannot
            // send multiple of level stat messages yet and need to free the unused string
            bm_free(borealis->stats.levels);
          }

          if (d.dt == 0) {
            count = 1;
          } else {
            count = (uint32_t)(d.dt_report / d.dt);
          }

          // Send to aggregate report every time this is reached
          err = borealis->send_spotter_log_aggregate(
              "borealis", count, "%.3f,%.3f,%.3f,%u,%.*s\n", d.dt, d.dt_report, d.max_iqr,
              d.first_band_index, d.levels_length, d.levels);

          if (err != BmOK) {
            bm_debug("Failed to send borealis aggregated log to spotter, reason: %d\n", err);
          }

          if (!borealis->m_aggregation_reports) {
            // Free levels information here as it will not be aggregated
            bm_free(d.levels);
          } else {
            borealis->stats = d;
          }
          err = BmOK;
        } else {
          err = BmEBADMSG;
        }
      } break;
      case BOREALIS_RECORDING_STATUS: {
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

/*!
 @brief Inform User If Borealis Level Statistics Report Interval Is Incorrect

 @details If the report interval for level statistics is higher than 1 per sampling duration
          (is less than half the sample duration on bridge's power controller),
          inform the user that it is not going to work as expected. If the report interval
          is zero (greater than the sample duration on bridge's power controller) report to
          the user that no reports will be sent to the backend. Ensure that the report interval
          is not set to zero as well.

 @param default_config Whether to use the default period or not
                       ref: level_statistics_default_period_s
 @param node_id Node ID of the system being evaluated
 @param period_s report_interval period in seconds
 */
static void borealisLevelsDurationEval(bool default_config, uint64_t node_id,
                                       float period_s = 300.0) {
  static constexpr float level_statistics_default_period_s = 300.0;
  power_config_s pwr_cfg = getPowerConfigs();
  float sample_duration_ms = static_cast<float>(
      pwr_cfg.subsampleEnabled ? pwr_cfg.subsampleDurationMs : pwr_cfg.sampleDurationMs);
  // Subtract 20 seconds as a buffer for BOREALIS boot, time it takes to get RTC and reporting time
  float recommended_sample_duration_s = sample_duration_ms / 1000.0 - 20.0;
  uint8_t num_level_stats_per_duration = 0;

  if (period_s == 0.0) {
    bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_WARNING, USE_HEADER,
                   "BOREALIS report_interval set to zero on %016" PRIx64
                   " , please reconfigure to: %f\n",
                   node_id, recommended_sample_duration_s);
    return;
  }

  if (recommended_sample_duration_s < 0.0) {
    bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_WARNING, USE_HEADER,
                   "Bridge sampleDurationMs too low for BOREALIS remote reporting, please "
                   "reconfigure to a higher value\n");
    return;
  }

  if (default_config) {
    bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_WARNING, USE_HEADER,
                   "Failed to get report_interval from BOREALIS on %016" PRIx64
                   " , using default value: %.3f\n",
                   node_id, level_statistics_default_period_s);
    period_s = level_statistics_default_period_s;
  }

  num_level_stats_per_duration = static_cast<uint8_t>(recommended_sample_duration_s / period_s);

  if (num_level_stats_per_duration > 1) {
    bridgeLogPrint(
        BRIDGE_SYS, BM_COMMON_LOG_LEVEL_WARNING, USE_HEADER,
        "Number of BOREALIS level statistics messages higher than recommended on %016" PRIx64
        ", multiple level statistics messages cannot be reported remotely"
        " only the last level statistics message will be reported, please change"
        " report_interval: Configured - %.3f, Recommended - %.3f\n",
        node_id, period_s, recommended_sample_duration_s);
  } else if (num_level_stats_per_duration == 0) {
    bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_WARNING, USE_HEADER,
                   "Number of BOREALIS level statistics messages is zero per reporting "
                   "interval on %016" PRIx64
                   ", level statistics data will not be reported remotely, please change "
                   "report_interval: Configured - %.3f, Recommended - %.3f\n",
                   node_id, period_s, recommended_sample_duration_s);
  }
}

/*!
 @brief Give Config Callback Handshake Semaphore

 @return true on success, false on failure
 */
static inline bool config_handshake_give(void) {
  bool ret = true;
  if (xSemaphoreGive(s_config_callback_handshake) != pdTRUE) {
    bm_debug("Failed to take give BOREALIS handshake semaphore\n");
    ret = false;
  }

  return ret;
}

/*!
 @brief Take Config Callback Handshake Semaphore

 @return true on success, false on failure
 */
static inline bool config_handshake_wait(void) {
  bool ret = true;
  static constexpr uint32_t callback_handshake_timeout_ms = 250;

  if (xSemaphoreTake(s_config_callback_handshake,
                     pdMS_TO_TICKS(callback_handshake_timeout_ms)) != pdTRUE) {
    bm_debug("Failed to take config handshake semaphore, BOREALIS may not have configuration "
             "value set\n");
    ret = false;
  }

  return ret;
}

/*!
 @brief Callback For Get Request To BOREALIS Reading Period In Milliseconds

 @details Obtains the reading period for BOREALIS spectrum and levels messages

 @param payload configuration value holding reading period 

 @return BmOK on success, BmErr on failure
 */
static BmErr borealisReadingPeriodConfigCb(uint8_t *payload) {
  BmErr err = BmENODATA;

  if (payload && s_current_sub) {
    BmConfigValue *msg = reinterpret_cast<BmConfigValue *>(payload);
    size_t size = sizeof(AbstractSensor::m_reading_period_ms);
    err = bcmp_config_decode_value(UINT32, msg->data, msg->data_length,
                                   &s_current_sub->m_reading_period_ms, &size);
  }

  config_handshake_give();

  return err;
}

/*!
 @brief Callback For Get Request To BOREALIS Level Statistics Interval In Seconds

 @details Obtains the level statistics calculation interval for BOREALIS

 @param payload configuration value holding calculation interval 

 @return BmOK on success, BmErr on failure
 */
static BmErr borealisLevelsStatisticsPeriodConfigCb(uint8_t *payload) {
  BmErr err = BmENODATA;
  float level_statistics_period_s = 0;
  bool default_config = false;

  if (payload && s_current_sub) {
    BmConfigValue *msg = reinterpret_cast<BmConfigValue *>(payload);
    size_t size = sizeof(level_statistics_period_s);
    err = bcmp_config_decode_value(FLOAT, msg->data, msg->data_length,
                                   &level_statistics_period_s, &size);
    if (err != BmOK) {
      default_config = true;
    }

    borealisLevelsDurationEval(default_config, s_current_sub->node_id,
                               level_statistics_period_s);
  }

  config_handshake_give();

  return err;
}

/*!
 @brief Creates A Borealis Sensor Subscriber

 @param node_id node ID of the subscriber
 @param reading_period_ms reading period of

 @return pointer to new borealis subscriber
 @return nullptr on failure
 */
Borealis_t *createBorealisSensorSub(uint64_t node_id) {
  BmErr err = BmOK;
  Borealis_t *ret = static_cast<Borealis_t *>(bm_malloc(sizeof(Borealis_t)));
  static constexpr uint32_t default_borealis_reading_period_ms = 1000;
  static constexpr char reading_period_key[] = "bandsSampleTimeMs";
  static constexpr char level_statistics_period_key[] = "report_interval";

  if (!s_config_callback_handshake) {
    s_config_callback_handshake = xSemaphoreCreateBinary();
  }

  if (ret) {
    ret = new (ret) Borealis_t();

    ret->_mutex = xSemaphoreCreateMutex();

    if (ret->_mutex) {
      ret->init();
      ret->node_id = node_id;
      ret->type = SENSOR_TYPE_BOREALIS;
      ret->next = nullptr;
      ret->m_reading_period_ms = default_borealis_reading_period_ms;
      s_current_sub = ret;
      bcmp_config_get(node_id, BM_CFG_PARTITION_SYSTEM, strlen(reading_period_key),
                      reading_period_key, &err, borealisReadingPeriodConfigCb);
      config_handshake_wait();
      bcmp_config_get(node_id, BM_CFG_PARTITION_SYSTEM, strlen(level_statistics_period_key),
                      level_statistics_period_key, &err,
                      borealisLevelsStatisticsPeriodConfigCb);
      if (!config_handshake_wait()) {
        borealisLevelsDurationEval(true, s_current_sub->node_id);
      }
      s_current_sub = NULL;
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
