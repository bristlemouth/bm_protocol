#pragma once

#include "abstractSensor.h"
#include "bm_borealis.h"
#include "cbor_sensor_report_encoder.h"

#include "util.h"

typedef struct BorealisSensor : public AbstractSensor {
public:
  bool subscribe() override;
  static constexpr uint32_t DEFAULT_BOREALIS_READING_PERIOD_MS = 1000;

private:
  static void borealisSubCallback(uint64_t node_id, const char *topic, uint16_t topic_len,
                                  const uint8_t *data, uint16_t data_len, uint8_t type,
                                  uint8_t version);
  static constexpr char subtag_spectrum[] = "/aos/borealis/spectrum";
  static constexpr char subtag_levels[] = "/aos/borealis/levels";
  static constexpr char subtag_level_statistics[] = "/aos/borealis/level_statistics";
  static constexpr char subtag_recstatus[] = "/aos/borealis/recstatus";
} Borealis_t;

typedef struct BorealisSensorLevelStatistics : public BorealisSensor {
public:
  typedef struct {
    uint8_t is_extended;
    uint8_t spl;
    uint8_t maq_iqr;
    uint8_t maq_iqr_band;
    uint8_t entropy;
    uint8_t spl_band_stats_size;
    uint8_t spl_bands_stats[0];
  } LevelStatisticsData_t;

  static constexpr LevelStatisticsData_t AOS_BOREALIS_NAN_AGG = {};

  void aggregate(void);
  static BmErr encode_sample(void *data, uint32_t sample_index,
                             sensor_report_encoder_context_t &context);

  struct borealis_level_statistics stats;

private:
  static constexpr uint8_t NUM_AOS_BOREALIS_FIELDS = 5;
  static constexpr uint8_t NUM_AOS_BOREALIS_FIELDS_EXTENDED = 6;
} BorealisLevelsStatistics_t;

Borealis_t *createBorealisSensorSub(abstractSensorType_e type, uint64_t node_id);
