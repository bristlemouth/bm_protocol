#pragma once

#include "abstractSensor.h"
#include "avgSampler.h"

#include <stdint.h>
#include <stdlib.h>

#define PME_DISSOLVED_OXYGEN_NUM_SAMPLE_MEMBERS 4

typedef struct pme_dissolved_oxygen_aggregations_s {
  double temperature_deg_c_mean;
  double do_mg_per_l_mean;
  double quality_mean;
  double do_saturation_pct_mean;
  uint32_t reading_count;
} pme_dissolved_oxygen_aggregations_t;

typedef struct PmeDissolvedOxygenSensor : public AbstractSensor {
  AveragingSampler temperature_deg_c;
  AveragingSampler do_mg_per_l;
  AveragingSampler quality;
  AveragingSampler do_saturation_pct;
  uint32_t reading_count;
  int8_t node_position;
  uint32_t last_timestamp;

  // 10 minutes
  static constexpr uint32_t DEFAULT_PME_DISSOLVED_READING_PERIOD_MS = 10 * 60 * 1000;
  static constexpr uint32_t N_SAMPLES_PAD = 2;
  static constexpr uint8_t MIN_READINGS_FOR_AGGREGATION = 1;

public:
  bool subscribe() override;
  void aggregate(void);

private:
  static void pmeDissolvedOxygenSubCallback(uint64_t node_id, const char *topic,
                                            uint16_t topic_len, const uint8_t *data,
                                            uint16_t data_len, uint8_t type, uint8_t version);

private:
  static constexpr char subtag[] = "/pme/do_data";
} PmeDissolvedOxygen_t;

PmeDissolvedOxygen_t *createPmeDissolvedOxygenSub(uint64_t node_id, uint32_t sample_duration_ms,
                                                  uint32_t subsample_interval_ms,
                                                  uint32_t subsample_duration_ms,
                                                  bool subsample_enabled);
