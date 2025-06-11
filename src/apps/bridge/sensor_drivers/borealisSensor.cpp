extern "C" {
#include "util.h"
}
#include "app_config.h"
#include "app_util.h"
#include "bm_config.h"
#include "bm_os.h"
#include "borealisSensor.h"
#include "bridgeLog.h"
#include "bristlemouth.h"
#include "ll.h"
#include "mbedtls_base64/base64.h"
#include "messages/config.h"
#include "pubsub.h"
#include "sensorController.h"
#include "spectral_entropy.h"
#include "uptime.h"
#include <inttypes.h>
#include <math.h>
#include <new>
#include <string.h>

#define MAX_BOREALIS_READING_PERIOD_MS(period) (period + 1000U)

// Only for 8-bit values in levels statistics, NOT for 12-bit values in levels
static constexpr float db_step_8bit = 0.75f;
static constexpr float db_min_8bit = -256.0f * db_step_8bit;

static constexpr float borealis_db_offset = 185.642f;
static struct BorealisSensor *s_current_sub = NULL;
static SemaphoreHandle_t s_config_callback_handshake = NULL;
static constexpr char subtag_spectrum[] = "/aos/borealis/spectrum";
static constexpr char subtag_levels[] = "/aos/borealis/levels";
static constexpr char subtag_level_statistics[] = "/aos/borealis/level_statistics";
static constexpr char subtag_recstatus[] = "/aos/borealis/recstatus";
static constexpr char subtag_hydrotwin_ldr[] = "/hydrotwin/ai/low-data-rate";
static constexpr char subtag_hydrotwin_hdr[] = "/hydrotwin/ai/high-data-rate";
static constexpr char config_borealis_tx_spl_threshold[] = "borealisTxSplThreshold";
static constexpr char config_borealis_tx_max_iqr_threshold[] = "borealisTxMaxIqrThreshold";
static constexpr char config_borealis_tx_entropy_threshold[] = "borealisTxEntropyThreshold";


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
  bool ret = true;
  char *sub = static_cast<char *>(bm_malloc(BM_TOPIC_MAX_LEN));
  const char *subtag = NULL;

  if (sub) {
    for (uint32_t i = 0; i < BOREALIS_SUB_COUNT && ret == true; i++) {
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
      case HYDROTWIN_LDR:
        subtag = subtag_hydrotwin_ldr;
        break;
      case HYDROTWIN_HDR:
        subtag = subtag_hydrotwin_hdr;
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

BmErr BorealisSensor::calculateQuantizedSplAndEntropy(uint8_t const *const band_stats,
                                                      const size_t band_stats_len, uint8_t &spl,
                                                      uint8_t &max_iqr_band, uint8_t &entropy) {
  spl = REPORT_NAN_ERROR_VALUE;
  max_iqr_band = REPORT_NAN_ERROR_VALUE;
  entropy = REPORT_NAN_ERROR_VALUE;
  if (band_stats == NULL || band_stats_len == 0) {
    clear_spectral_entropy_list();
    return BmEINVAL;
  }

  constexpr unsigned int num_stats = 4; // 25%, 50%, 75%, mean
  const unsigned int num_bands = band_stats_len / num_stats;

  uint8_t max_iqr_so_far = 0;
  float spl_linear_sum = 0.0f;
  float *means = static_cast<float *>(bm_malloc(num_bands * sizeof(float)));
  if (means == NULL) {
    bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_ERROR, USE_HEADER,
                   "Failed to allocate for Borealis level stats means\n");
    clear_spectral_entropy_list();
    return BmENOMEM;
  }
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
    means[band_index] = band_stats[offset + 3] * db_step_8bit + db_min_8bit;
    spl_linear_sum += powf(10.0f, means[band_index] / 10.0f);
  }

  float broadband_spl_db = 10.0f * log10f(spl_linear_sum);
  spl = fmaxf(0.0f, fminf(255.0f, (broadband_spl_db - db_min_8bit) / db_step_8bit + 0.5f));

  float min_entropy = calc_min_spectral_entropy_and_clear_list(num_bands, means);
  entropy = fmaxf(0.0f, fminf(255.0f, min_entropy * 255.0f));

  bm_free(means);
  return BmOK;
}

/*!
  @brief Determine whether to send extended data based on configured thresholds
  @return true if the data exceeds configured thresholds, false otherwise
*/
bool BorealisSensor::exceedsConfiguredThresholds(uint8_t spl, uint8_t max_iqr,
                                                 uint8_t entropy) {
  float spl_threshold = 0.0f;
  get_config_float(BM_CFG_PARTITION_SYSTEM, config_borealis_tx_spl_threshold,
                   strlen(config_borealis_tx_spl_threshold), &spl_threshold);
  float max_iqr_threshold = 0.0f;
  get_config_float(BM_CFG_PARTITION_SYSTEM, config_borealis_tx_max_iqr_threshold,
                   strlen(config_borealis_tx_max_iqr_threshold), &max_iqr_threshold);
  float entropy_threshold = 1.0f;
  get_config_float(BM_CFG_PARTITION_SYSTEM, config_borealis_tx_entropy_threshold,
                   strlen(config_borealis_tx_entropy_threshold), &entropy_threshold);

  bool spl_exceeds = spl * db_step_8bit + db_min_8bit + borealis_db_offset >= spl_threshold;
  bool max_iqr_exceeds = max_iqr * db_step_8bit >= max_iqr_threshold;
  bool entropy_exceeds = entropy <= entropy_threshold * 255.0f;
  return (spl_exceeds || max_iqr_exceeds || entropy_exceeds);
}

/*!
 @brief Aggregate Data To Send To Report Builder

 @details This takes the latest level statistics messages calculates SPL and entropy,
          as well as adds the message BorealisAggregationData_t to the report builder.
 */
void BorealisSensor::aggregate(void) {
  if (xSemaphoreTake(_mutex, portMAX_DELAY) == pdTRUE) {
    size_t decoded_byte_len = 0;
    BorealisAggregationData_t report = {};
    report.max_iqr = tracking_data.stats.max_iqr;

    mbedtls_base64_decode(NULL, 0, &decoded_byte_len,
                          (const unsigned char *)tracking_data.stats.levels,
                          tracking_data.stats.levels_length);

    report.is_extended = false;
    report.spl_band_stats = static_cast<uint8_t *>(bm_malloc(decoded_byte_len));
    if (report.spl_band_stats) {
      int err = mbedtls_base64_decode(
          report.spl_band_stats, decoded_byte_len, (size_t *)&report.spl_band_stats_size,
          (const unsigned char *)tracking_data.stats.levels, tracking_data.stats.levels_length);
      if (err != 0) {
        bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_ERROR, USE_HEADER,
                       "Failed to base64 decode Borealis level statistics. "
                       "Probably invalid character. err=%d, len=%lu, b64=%.*s\n",
                       err, tracking_data.stats.levels_length,
                       tracking_data.stats.levels_length, tracking_data.stats.levels);
      } else {
        calculateQuantizedSplAndEntropy(report.spl_band_stats, report.spl_band_stats_size,
                                        report.spl, report.max_iqr_band, report.entropy);
        report.max_iqr_band += tracking_data.stats.first_band_index;
        report.is_extended =
            exceedsConfiguredThresholds(report.spl, report.max_iqr, report.entropy);
      }
    }

    // Add hydrotwin LDR
    if (tracking_data.ldr.size) {
      report.hydrotwin_ldr.buf = static_cast<uint8_t *>(bm_malloc(tracking_data.ldr.size));
      if (report.hydrotwin_ldr.buf) {
        memcpy(report.hydrotwin_ldr.buf, tracking_data.ldr.buf, tracking_data.ldr.size);
        report.hydrotwin_ldr.size = tracking_data.ldr.size;
      } else {
        bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_ERROR, USE_HEADER,
                       "Failed to allocate memory for Hydrotwin LDR buffer in %s\n", __func__);
      }
    }

    reportBuilderAddToQueue(node_id, SENSOR_TYPE_BOREALIS, &report,
                            sizeof(BorealisAggregationData_t), REPORT_BUILDER_SAMPLE_MESSAGE);
  } else {
    bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_ERROR, USE_HEADER,
                   "Failed to allocate memory for Borealis level statistics in %s\n", __func__);
  }

  // Free data
  bm_free(tracking_data.stats.levels);
  bm_free(tracking_data.ldr.buf);
  tracking_data = (struct AggregateTrackingData){};

  xSemaphoreGive(_mutex);
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
  static constexpr uint8_t NUM_AOS_BOREALIS_FIELDS_MIN = 5;
  BmErr err = BmOK;
  BorealisAggregationData_t borealis_data =
      (static_cast<BorealisAggregationData_t *>(data))[sample_index];
  uint8_t num_fields = NUM_AOS_BOREALIS_FIELDS_MIN;

  // If data is extended increase number of fields
  if (borealis_data.is_extended) {
    num_fields++;
  }

  // If hydrotwin data is available increase number of fields
  if (borealis_data.hydrotwin_ldr.size) {
    num_fields++;
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
            borealis_data.spl_band_stats_size) != CborNoError) ||
       (borealis_data.hydrotwin_ldr.size &&
        sensor_report_encoder_add_sample_member(
            context, encode_buffer_sample_member, borealis_data.hydrotwin_ldr.buf,
            borealis_data.hydrotwin_ldr.size) != CborNoError))) {
    bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_ERROR, USE_HEADER,
                   "Failed to add borealis level statistics sample member in %s\n", __func__);
    err = BmEBADMSG;
  }

  if (err == BmOK && sensor_report_encoder_close_sample(context) != CborNoError) {
    bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_ERROR, USE_HEADER,
                   "Failed to close sample in %s\n", __func__);
    err = BmEBADMSG;
  }

  // Free band stats and hydrotwin data
  bm_free(borealis_data.spl_band_stats);
  bm_free(borealis_data.hydrotwin_ldr.buf);

  return err;
}

/*!
 @brief Log Hydrotwin Data To Spotter SD Card

 @param data Data to log to SD card
 @param data_len Length of data
 @param type BorealisSubscription_t, must be HYDROTWIN_LDR or HYDROTWIN_HDR

 @return BmOK on success, BMErr on failure
 */
BmErr BorealisSensor::hydrotwinSendSpotterLog(const uint8_t *data, uint16_t data_len,
                                              BorealisSubscriptions_t type) {
  BmErr err = BmEINVAL;
  size_t encoded_data_len_check = 0;

  mbedtls_base64_encode(NULL, 0, &encoded_data_len_check, (const unsigned char *)data,
                        data_len);

  // Base64 encoded data
  if (encoded_data_len_check) {
    uint8_t *encoded_data = static_cast<uint8_t *>(bm_malloc(encoded_data_len_check));
    err = BmENOMEM;

    if (encoded_data) {
      size_t encoded_data_len = 0;
      mbedtls_base64_encode(encoded_data, encoded_data_len_check, &encoded_data_len,
                            (const unsigned char *)data, data_len);
      if (type == HYDROTWIN_LDR) {
        err = send_spotter_log_aggregate("hydrotwin", m_hydrotwin_ldr_minute, "%.*s\n",
                                         encoded_data_len, encoded_data);
      } else if (type == HYDROTWIN_HDR) {
        static constexpr uint8_t hydrotwin_message_version = 1;
        SensorHeaderMsg::Data header = {
            hydrotwin_message_version,
            uptimeGetMs(),
            bm_ticks_to_ms(bm_get_tick_count()),
            0,
        };
        err = send_spotter_log_individual(
            "hydrotwin", header,
            MAX_BOREALIS_READING_PERIOD_MS(MIN_TO_MS(m_hydrotwin_hdr_minute)), "%.*s\n",
            encoded_data_len, encoded_data);
      }
      bm_free(encoded_data);
    }
  }

  if (err != BmOK) {
    bm_debug("Failed to send hydrotwin log %d to spotter, reason: %d\n", type, err);
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
  BorealisSubscriptions_t sub_type = BOREALIS_SUB_COUNT;

  if (strstr(topic, subtag_spectrum) != NULL) {
    sub_type = BOREALIS_SPECTRUM;
  } else if (strstr(topic, subtag_levels) != NULL) {
    sub_type = BOREALIS_LEVELS;
  } else if (strstr(topic, subtag_level_statistics) != NULL) {
    sub_type = BOREALIS_LEVEL_STATISTICS;
  } else if (strstr(topic, subtag_recstatus) != NULL) {
    sub_type = BOREALIS_RECORDING_STATUS;
  } else if (strstr(topic, subtag_hydrotwin_ldr) != NULL) {
    sub_type = HYDROTWIN_LDR;
  } else if (strstr(topic, subtag_hydrotwin_hdr) != NULL) {
    sub_type = HYDROTWIN_HDR;
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
          // Base64 decodes to 3/4 of input size.
          // There are two 12-bit band values per 3 bytes.
          // 3/4 * 2/3 = 1/2
          size_t num_bands = d.levels_length / 2;
          float *spl_db = static_cast<float *>(bm_malloc(num_bands * sizeof(float)));
          if (!spl_db) {
            bm_debug("Failed to allocate memory for Borealis levels data\n");
            err = BmENOMEM;
            bm_free(d.levels);
            break;
          }
          parse_levels(spl_db, d.levels, d.levels_length);
          bm_free(d.levels);
          insert_spectrum_into_list(num_bands, spl_db);
          bm_free(spl_db);
        }
      } break;
      case BOREALIS_LEVEL_STATISTICS: {
        struct borealis_level_statistics d;
        if (borealis_levels_statistics_decode(&d, (uint8_t *)data, data_len) == CborNoError) {
          uint32_t count = 0;

          if (borealis->tracking_data.stats.levels) {
            // This would be an indication that the user has configured BOREALIS' report_interval
            // to be less than half of the bridge power controller's sampleDurationMs, we cannot
            // send multiple of level stat messages yet and need to free the unused string
            bm_free(borealis->tracking_data.stats.levels);
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
            borealis->tracking_data.stats = d;
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
      case HYDROTWIN_LDR: {
        // Clear the last buffer if it has not been consumed
        if (borealis->tracking_data.ldr.buf) {
          bm_free(borealis->tracking_data.ldr.buf);
          borealis->tracking_data.ldr.buf = NULL;
          borealis->tracking_data.ldr.size = 0;
        }

        err = borealis->hydrotwinSendSpotterLog(data, data_len, sub_type);
        if (borealis->m_aggregation_reports) {
          borealis->tracking_data.ldr.buf = static_cast<uint8_t *>(bm_malloc(data_len));

          if (borealis->tracking_data.ldr.buf) {
            memcpy(borealis->tracking_data.ldr.buf, data, data_len);
            borealis->tracking_data.ldr.size = data_len;
            err = BmOK;
          } else {
            err = BmENOMEM;
          }
        }
      } break;
      case HYDROTWIN_HDR: {
        err = borealis->hydrotwinSendSpotterLog(data, data_len, sub_type);
      } break;
      default:
        break;
      }
      if (err != BmOK) {
        bm_debug("Failed to handle Borealis subscription, reason: %d\n", err);
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
 @brief Callback For Get Request To BOREALIS Hydrotwin HDR Frequency Config

 @param payload configuration value holding reading period

 @return BmOK on success, BmErr on failure
 */
static BmErr hydrotwinHdrConfigCb(uint8_t *payload) {
  BmErr err = BmENODATA;

  if (payload && s_current_sub) {
    BmConfigValue *msg = reinterpret_cast<BmConfigValue *>(payload);
    size_t size = sizeof(BorealisSensor::m_hydrotwin_hdr_minute);
    err = bcmp_config_decode_value(UINT32, msg->data, msg->data_length,
                                   &s_current_sub->m_hydrotwin_hdr_minute, &size);
  }

  config_handshake_give();

  return err;
}

/*!
 @brief Callback For Get Request To BOREALIS Hydrotwin LDR Frequency Config

 @param payload configuration value holding reading period

 @return BmOK on success, BmErr on failure
 */
static BmErr hydrotwinLdrConfigCb(uint8_t *payload) {
  BmErr err = BmENODATA;

  if (payload && s_current_sub) {
    BmConfigValue *msg = reinterpret_cast<BmConfigValue *>(payload);
    size_t size = sizeof(BorealisSensor::m_hydrotwin_ldr_minute);
    err = bcmp_config_decode_value(UINT32, msg->data, msg->data_length,
                                   &s_current_sub->m_hydrotwin_ldr_minute, &size);
  }

  config_handshake_give();

  return err;
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
  static constexpr char hydrotwin_ldr_key[] = "hydrotwin_ldr";
  static constexpr char hydrotwin_hdr_key[] = "hydrotwin_hdr";
  static constexpr uint32_t hydrotwin_ldr_minute_default = 12;
  static constexpr uint32_t hydrotwin_hdr_minute_default = 1;

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
      ret->m_hydrotwin_ldr_minute = hydrotwin_ldr_minute_default;
      ret->m_hydrotwin_hdr_minute = hydrotwin_hdr_minute_default;
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
      bcmp_config_get(node_id, BM_CFG_PARTITION_SYSTEM, strlen(hydrotwin_ldr_key),
                      hydrotwin_ldr_key, &err, hydrotwinLdrConfigCb);
      config_handshake_wait();
      bcmp_config_get(node_id, BM_CFG_PARTITION_SYSTEM, strlen(hydrotwin_hdr_key),
                      hydrotwin_hdr_key, &err, hydrotwinHdrConfigCb);
      config_handshake_wait();
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

inline uint8_t BorealisSensor::unpack_nibble(const uint8_t *bytes, size_t nibble_index) {
  const size_t byte_index = nibble_index / 2;
  const size_t shift_bits = (nibble_index % 2) * 4;
  return (bytes[byte_index] >> shift_bits) & 0xF;
}

inline uint16_t BorealisSensor::unpack_12bit(const uint8_t *bytes, size_t nibble_index) {
  return unpack_nibble(bytes, nibble_index + 0) << 0 |
         unpack_nibble(bytes, nibble_index + 1) << 4 |
         unpack_nibble(bytes, nibble_index + 2) << 8;
}

void BorealisSensor::parse_levels(float *spl_db, const char *levels_as_base64,
                                  size_t levels_length) {
  size_t out_len = 0;
  mbedtls_base64_decode(NULL, 0, &out_len, reinterpret_cast<const uint8_t *>(levels_as_base64),
                        levels_length);
  uint8_t raw_bytes[out_len];
  int err =
      mbedtls_base64_decode(raw_bytes, sizeof(raw_bytes), &out_len,
                            reinterpret_cast<const uint8_t *>(levels_as_base64), levels_length);
  if (err != 0) {
    bm_debug("Failed to decode base64 levels: %d\n", err);
    return;
  }

  // These constants are for 12-bit values, unlike the 8-bit ones at the top of this file.
  constexpr float cstep = 0.046875f;                            // dB
  constexpr float clow = -4096.0f * cstep + borealis_db_offset; // dB
  uint32_t num_bands = out_len * 2U / 3U;                       // 3 bytes -> 2 12-bit values
  for (uint32_t i = 0; i < num_bands; i++) {
    spl_db[i] = unpack_12bit(raw_bytes, i * 3) * cstep + clow;
  }
}
