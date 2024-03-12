#pragma once
#include "FreeRTOS.h"
#include "abstractSensor.h"
#include "avgSampler.h"
#include "sensorController.h"

#include <stdint.h>
#include <stdlib.h>

#define SOFT_NUM_SAMPLE_MEMBERS 1

typedef struct soft_aggregations_s {
  double temp_mean_deg_c;
  uint32_t reading_count;
} soft_aggregations_t;

typedef struct SoftSensor : public AbstractSensor {
  uint32_t current_agg_period_ms;
  AveragingSampler temp_deg_c;
  uint32_t reading_count;
  int8_t node_position;
  uint32_t last_timestamp;

  // Extra sample padding to account for timing slop. Calculated as the sample frequency + 2 minutes bridge on period + some extra slop.
  // 2 minutes is the minimum bridge on period and the soft by default is sampling at 2Hz. So 2*120 + 30 = 270.
  static constexpr uint32_t N_SAMPLES_PAD = 270;
  static constexpr uint8_t MIN_READINGS_FOR_AGGREGATION = 3;
  static constexpr double TEMP_SAMPLE_MEMBER_MIN = -5;
  static constexpr double TEMP_SAMPLE_MEMBER_MAX = 50;

public:
  bool subscribe() override;
  void aggregate(void);

private:
  static void softSubCallback(uint64_t node_id, const char *topic, uint16_t topic_len,
                             const uint8_t *data, uint16_t data_len, uint8_t type,
                             uint8_t version);

private:
  static constexpr char subtag[] = "/sofar/bm_soft_temp";
} Soft_t;

Soft_t* createSoftSub(uint64_t node_id, uint32_t current_agg_period_ms, uint32_t averager_max_samples);
