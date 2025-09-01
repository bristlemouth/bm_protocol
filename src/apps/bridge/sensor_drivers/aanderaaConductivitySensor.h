#pragma once
#include "FreeRTOS.h"
#include "abstractSensor.h"
#include "avgSampler.h"
#include "sensorController.h"
#include <cmath>
#include <stdint.h>
#include <stdlib.h>

#define AANDERAA_CONDUCTIVITY_NUM_SAMPLE_MEMBERS 6

typedef struct aanderaa_conductivity_aggregations_s {
  double conductivity_mean_ms_cm;
  double temperature_mean_deg_c;
  double salinity_mean_psu;
  double water_density_mean_kg_m3;
  double sound_speed_mean_m_s;
  double depth_mean_m;
  uint32_t reading_count;
} aanderaa_conductivity_aggregations_t;

typedef struct AanderaaConductivitySensor : public AbstractSensor {
  uint32_t current_agg_period_ms;
  AveragingSampler conductivity_ms_cm;
  AveragingSampler temperature_deg_c;
  AveragingSampler salinity_psu;
  AveragingSampler water_density_kg_m3;
  AveragingSampler sound_speed_m_s;
  AveragingSampler depth_m;
  uint32_t reading_count;
  int8_t node_position;
  uint32_t last_timestamp;

  // Extra sample padding to account for timing slop. Calculated as the sample frequency + 2 minutes bridge on period + some extra slop.
  // 2 minutes is the minimum bridge on period and the conductivity sensor by default is sampling at 1Hz. So 1*120 + 30 = 150.
  static constexpr uint32_t N_SAMPLES_PAD = 150;
  static constexpr uint8_t MIN_READINGS_FOR_AGGREGATION = 3;

  AanderaaConductivitySensor()
      : conductivity_ms_cm(), temperature_deg_c(), salinity_psu(), water_density_kg_m3(),
        sound_speed_m_s(), depth_m(), reading_count(0), node_position(-1), last_timestamp(0) {}

public:
  bool subscribe() override;
  void aggregate(void);
  static constexpr aanderaa_conductivity_aggregations_t aanderaa_conductivity_NAN_AGG = {
      .conductivity_mean_ms_cm = NAN,
      .temperature_mean_deg_c = NAN,
      .salinity_mean_psu = NAN,
      .water_density_mean_kg_m3 = NAN,
      .sound_speed_mean_m_s = NAN,
      .depth_mean_m = NAN,
      .reading_count = 0,
  };

private:
  static void aanderaaConductivitySubCallback(uint64_t node_id, const char *topic,
                                              uint16_t topic_len, const uint8_t *data,
                                              uint16_t data_len, uint8_t type, uint8_t version);

private:
  static constexpr char subtag[] = "/sofar/aanderaa_conductivity_data";
} AanderaaConductivity_t;

AanderaaConductivity_t *createAanderaaConductivitySub(uint64_t node_id, uint32_t agg_period_ms,
                                                      uint32_t averager_max_samples);