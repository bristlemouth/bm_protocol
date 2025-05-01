#pragma once

#include "FreeRTOS.h"
#include "abstractSensor.h"
#include "bm_borealis.h"
#include "cbor_sensor_report_encoder.h"
#include "semphr.h"

#include "util.h"

typedef struct {
  uint8_t is_extended;
  uint8_t spl;
  uint8_t max_iqr;
  uint8_t max_iqr_band;
  uint8_t entropy;
  uint32_t spl_band_stats_size;
  uint8_t spl_band_stats[0];
} BorealisLevelStatisticsData_t;

typedef enum {
  BOREALIS_SPECTRUM,
  BOREALIS_LEVELS,
  BOREALIS_LEVEL_STATISTICS,
  BOREALIS_RECORDING_STATUS,
  BOREALIS_SUB_COUNT,
} BorealisSubscriptions_t;

typedef struct BorealisSensor : public AbstractSensor {
public:
  void init(void);
  bool subscribe() override;
  void aggregate(void);
  static BmErr encode_sample(void *data, uint32_t sample_index,
                             sensor_report_encoder_context_t &context);
  bool m_aggregation_reports;
  static constexpr float LEVEL_STATISTICS_DEFAULT_PERIOD_S = 300.0;
  static constexpr BorealisLevelStatisticsData_t AOS_BOREALIS_NAN_AGG = {
      .is_extended = 0,
      .spl = 0xFF,
      .max_iqr = 0xFF,
      .max_iqr_band = 0xFF,
      .entropy = 0xFF,
      .spl_band_stats_size = 0,
      {},
  };

private:
  struct borealis_level_statistics stats;
  static void borealisSubCallback(uint64_t node_id, const char *topic, uint16_t topic_len,
                                  const uint8_t *data, uint16_t data_len, uint8_t type,
                                  uint8_t version);
  static uint32_t s_borealis_level_stats_max_size;
} Borealis_t;

Borealis_t *createBorealisSensorSub(uint64_t node_id);
