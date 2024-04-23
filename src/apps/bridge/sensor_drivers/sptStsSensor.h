#pragma once
#include "FreeRTOS.h"
#include "abstractSensor.h"
#include "avgSampler.h"
#include "sensorController.h"

#include <stdint.h>
#include <stdlib.h>

#define SPT_STS_NUM_SAMPLE_MEMBERS 2

typedef struct spt_sts_aggregations_s {
  double turbidity_s_mean_ftu;
  double turbidity_r_mean_ftu;
  uint32_t reading_count;
} spt_sts_aggregations_t;

typedef struct SptStsSensor : public AbstractSensor {
  uint32_t agg_period_ms;
  AveragingSampler turbidity_s_ftu;
  AveragingSampler turbidity_r_ftu;
  uint32_t reading_count;
  int8_t node_position;
  uint32_t last_timestamp;


  // TODO - recalculate these values AND the MIN/MAX values - see the spec sheet
  // Extra sample padding to account for timing slop. Calculated as the sample frequency + 2 minutes bridge on period + some extra slop.
  // 2 minutes is the minimum bridge on period and the soft by default is sampling at 2Hz. So 2*120 + 30 = 270.
  static constexpr uint32_t N_SAMPLES_PAD = 270;
  static constexpr uint8_t MIN_READINGS_FOR_AGGREGATION = 3;
  static constexpr double S_SAMPLE_MEMBER_MIN = -20;
  static constexpr double S_SAMPLE_MEMBER_MAX = 61.88;
  static constexpr double R_SAMPLE_MEMBER_MIN = -20;
  static constexpr double R_SAMPLE_MEMBER_MAX = 61.88;

public:
  bool subscribe() override;
  void aggregate(void);

private:
  static void sptStsSubCallback(uint64_t node_id, const char *topic, uint16_t topic_len,
                             const uint8_t *data, uint16_t data_len, uint8_t type,
                             uint8_t version);

private:
  static constexpr char subtag[] = "/sofar/spt_sts_data";
} SptSts_t;

SptSts_t* createSptStsSub(uint64_t node_id, uint32_t agg_period_ms, uint32_t averager_max_samples);
