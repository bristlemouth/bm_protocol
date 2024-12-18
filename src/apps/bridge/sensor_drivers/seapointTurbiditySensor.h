#pragma once
#include "FreeRTOS.h"
#include "abstractSensor.h"
#include "avgSampler.h"
#include "sensorController.h"

#include <stdint.h>
#include <stdlib.h>

#define SEAPOINT_TURBIDITY_NUM_SAMPLE_MEMBERS 2

typedef struct seapoint_turbidity_aggregations_s {
  double turbidity_s_mean_ftu;
  double turbidity_r_mean_ftu;
  uint32_t reading_count;
} seapoint_turbidity_aggregations_t;

typedef struct SeapointTurbiditySensor : public AbstractSensor {
  uint32_t agg_period_ms;
  AveragingSampler turbidity_s_ftu;
  AveragingSampler turbidity_r_ftu;
  uint32_t reading_count;
  int8_t node_position;
  uint32_t last_timestamp;

  // Extra sample padding to account for timing slop. Calculated as the sample frequency + 2 minutes bridge on period + some extra slop.
  // 2 minutes is the minimum bridge on period and the turbidity sensor by default is sampling at 1Hz. So 1*120 + 30 = 150.
  static constexpr uint32_t N_SAMPLES_PAD = 150;
  static constexpr uint8_t MIN_READINGS_FOR_AGGREGATION = 3;

public:
  bool subscribe() override;
  void aggregate(void);

private:
  static void seapointTurbiditySubCallback(uint64_t node_id, const char *topic,
                                           uint16_t topic_len, const uint8_t *data,
                                           uint16_t data_len, uint8_t type, uint8_t version);

private:
  static constexpr char subtag[] = "/sofar/seapoint_turbidity_data";
} SeapointTurbidity_t;

SeapointTurbidity_t *createSeapointTurbiditySub(uint64_t node_id, uint32_t agg_period_ms,
                                                uint32_t averager_max_samples);
