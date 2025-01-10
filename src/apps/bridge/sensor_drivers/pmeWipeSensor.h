#pragma once

#include "abstractSensor.h"
#include "avgSampler.h"

#include <stdint.h>
#include <stdlib.h>

#define PME_WIPE_NUM_SAMPLE_MEMBERS 2

typedef struct pme_wipe_aggregations_s {
  double wipe_current_mean_ma;
  double wipe_duration_s;
  uint32_t reading_count;
} pme_wipe_aggregations_t;

typedef struct PmeWipeSensor : public AbstractSensor {
  uint32_t agg_period_ms;
  AveragingSampler wipe_current_ma;
  AveragingSampler wipe_duration_s;
  uint32_t reading_count;
  int8_t node_position;
  uint32_t last_timestamp;

  // 4 hours default wiper reading period
  static constexpr uint32_t DEFAULT_PME_WIPER_READING_PERIOD_MS = 4 * 60 * 60 * 1000;
  static constexpr uint32_t N_SAMPLES_PAD = 2;
  static constexpr uint8_t MIN_READINGS_FOR_AGGREGATION = 1;

public:
  bool subscribe() override;
  void aggregate(void);

private:
  static void pmeWipeSubCallback(uint64_t node_id, const char *topic,
                                 uint16_t topic_len, const uint8_t *data,
                                 uint16_t data_len, uint8_t type, uint8_t version);

private:
  static constexpr char subtag[] = "/pme/wiper";
} PmeWipe_t;

PmeWipe_t *createPmeWipeSub(uint64_t node_id, uint32_t sample_duration_ms);