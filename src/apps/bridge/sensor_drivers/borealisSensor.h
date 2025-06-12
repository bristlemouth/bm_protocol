#pragma once

#include "FreeRTOS.h"
#include "abstractSensor.h"
#include "bm_borealis.h"
#include "cbor_sensor_report_encoder.h"
#include "semphr.h"

#include "util.h"

typedef struct {
  uint32_t size;
  uint8_t *buf;
} HydrotwinLdrData_t;

typedef struct {
  uint8_t is_extended;
  uint8_t spl;
  uint8_t max_iqr;
  uint8_t max_iqr_band;
  uint8_t entropy;
  uint32_t spl_band_stats_size;
  uint8_t *spl_band_stats;
  HydrotwinLdrData_t hydrotwin_ldr;
} BorealisAggregationData_t;

typedef enum {
  BOREALIS_SPECTRUM,
  BOREALIS_LEVELS,
  BOREALIS_LEVEL_STATISTICS,
  BOREALIS_RECORDING_STATUS,
  HYDROTWIN_LDR,
  HYDROTWIN_HDR,
  BOREALIS_SUB_COUNT,
} BorealisSubscriptions_t;

typedef struct BorealisSensor : public AbstractSensor {
public:
  // public instance methods
  void init(void);
  bool subscribe() override;
  void aggregate(void);

  // public static methods
  static BmErr encode_sample(void *data, uint32_t sample_index,
                             sensor_report_encoder_context_t &context);

  // public instance variables
  bool m_aggregation_reports;
  uint32_t m_hydrotwin_ldr_minute;
  uint32_t m_hydrotwin_hdr_minute;

  // public static constants
  static constexpr uint8_t REPORT_NAN_ERROR_VALUE = 0xFF;
  static constexpr BorealisAggregationData_t AOS_BOREALIS_NAN_AGG = {
      .is_extended = 0,
      .spl = REPORT_NAN_ERROR_VALUE,
      .max_iqr = REPORT_NAN_ERROR_VALUE,
      .max_iqr_band = REPORT_NAN_ERROR_VALUE,
      .entropy = REPORT_NAN_ERROR_VALUE,
      .spl_band_stats_size = 0,
      .spl_band_stats = NULL,
      .hydrotwin_ldr = {.size = 0, .buf = NULL},
  };

private:
  // private instance methods
  BmErr hydrotwinSendSpotterLog(const uint8_t *data, uint16_t data_len,
                                BorealisSubscriptions_t type);

  // private instance variables
  struct AggregateTrackingData {
    struct borealis_level_statistics stats;
    HydrotwinLdrData_t ldr;
  } tracking_data;

  // private static methods
  static void borealisSubCallback(uint64_t node_id, const char *topic, uint16_t topic_len,
                                  const uint8_t *data, uint16_t data_len, uint8_t type,
                                  uint8_t version);
  static bool exceedsConfiguredThresholds(uint8_t spl, uint8_t max_iqr, uint8_t entropy);
  static BmErr calculateQuantizedSplAndEntropy(uint8_t const *const band_stats,
                                               const size_t band_stats_len, uint8_t &spl,
                                               uint8_t &max_iqr_band, uint8_t &entropy);
  static inline uint8_t unpack_nibble(const uint8_t *bytes, size_t nibble_index);
  static inline uint16_t unpack_12bit(const uint8_t *bytes, size_t nibble_index);
  static void parse_levels(float *spl_db, const char *levels_as_base64, size_t levels_length);
} Borealis_t;

Borealis_t *createBorealisSensorSub(uint64_t node_id);
