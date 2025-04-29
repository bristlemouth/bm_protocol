#pragma once

#include "FreeRTOS.h"
#include "abstractSensor.h"
#include "bm_borealis.h"
#include "cbor_sensor_report_encoder.h"
#include "semphr.h"

#include "util.h"

typedef struct BorealisSensor : public AbstractSensor {
public:
  static void init(void);
  static inline bool config_handshake_give(void);
  static inline bool config_handshake_wait(void);
  bool subscribe() override;
  static bool s_aggregation_reports;
  static SemaphoreHandle_t s_config_callback_handshake;
  static constexpr uint32_t DEFAULT_BOREALIS_READING_PERIOD_MS = 1000;
  static constexpr uint32_t CALLBACK_HANDSHAKE_TIMEOUT_MS = 250;
  static constexpr float LEVEL_STATISTICS_DEFAULT_PERIOD_S = 300.0;
  static constexpr char READING_PERIOD_KEY[] = "bandsSampleTimeMs";
  static constexpr char LEVEL_STATISTICS_PERIOD_KEY[] = "report_interval";

private:
  static void borealisSubCallback(uint64_t node_id, const char *topic, uint16_t topic_len,
                                  const uint8_t *data, uint16_t data_len, uint8_t type,
                                  uint8_t version);
  static constexpr char subtag_spectrum[] = "/aos/borealis/spectrum";
  static constexpr char subtag_levels[] = "/aos/borealis/levels";
  static constexpr char subtag_level_statistics[] = "/aos/borealis/level_statistics";
  static constexpr char subtag_recstatus[] = "/aos/borealis/recstatus";

  static constexpr uint32_t MAX_SAMPLES_PER_REPORT = 2;
} Borealis_t;

typedef struct BorealisSensorLevelStatistics : public BorealisSensor {
public:
  typedef struct {
    uint8_t is_extended;
    uint8_t spl;
    uint8_t max_iqr;
    uint8_t max_iqr_band;
    uint8_t entropy;
    uint32_t spl_band_stats_size;
    uint8_t spl_bands_stats[0];
  } LevelStatisticsData_t;

  static constexpr LevelStatisticsData_t AOS_BOREALIS_NAN_AGG = {};

  void aggregate(void);
  static BmErr encode_sample(void *data, uint32_t sample_index,
                             sensor_report_encoder_context_t &context);

  struct borealis_level_statistics stats;

  BorealisSensorLevelStatistics() : stats({}) {}

private:
  static uint32_t s_borealis_level_stats_max_size;
  static constexpr uint8_t NUM_AOS_BOREALIS_FIELDS = 5;
  static constexpr uint8_t NUM_AOS_BOREALIS_FIELDS_EXTENDED = 6;
} BorealisLevelsStatistics_t;

Borealis_t *createBorealisSensorSub(abstractSensorType_e type, uint64_t node_id);
