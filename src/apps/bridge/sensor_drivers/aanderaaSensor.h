#pragma once
#include "FreeRTOS.h"
#include "abstractSensor.h"
#include <stdint.h>
#include <stdlib.h>
#include "avgSampler.h"
#include "sensorController.h"

#define AANDERAA_NUM_SAMPLE_MEMBERS 8

// Clang doesn't have M_TWOPI defined in math.h so for CI tests we define it here.
#ifdef CI_TEST
#define M_TWOPI 2*M_PI
#endif

typedef struct aanderaa_aggregations_s {
  double abs_speed_mean_cm_s;
  double abs_speed_std_cm_s;
  double direction_circ_mean_rad;
  double direction_circ_std_rad;
  double temp_mean_deg_c;
  double abs_tilt_mean_rad;
  double std_tilt_mean_rad;
  uint32_t reading_count;
} aanderaa_aggregations_t;

typedef struct AanderaaSensor : public AbstractSensor {
  uint32_t current_agg_period_ms;
  AveragingSampler abs_speed_cm_s;
  AveragingSampler direction_rad;
  AveragingSampler temp_deg_c;
  AveragingSampler abs_tilt_rad;
  AveragingSampler std_tilt_rad;
  uint32_t reading_count;

  static constexpr uint32_t N_SAMPLES_PAD =
      10; // Extra sample padding to account for timing slop.
  static constexpr uint8_t MIN_READINGS_FOR_AGGREGATION = 3;
  static constexpr double DIRECTION_SAMPLE_MEMBER_MIN = 0.0;
  static constexpr double DIRECTION_SAMPLE_MEMBER_MAX = M_TWOPI;
  static constexpr double TILT_SAMPLE_MEMBER_MIN = 0.0;
  static constexpr double TILT_SAMPLE_MEMBER_MAX = M_PI_2;
  static constexpr double ABS_SPEED_SAMPLE_MEMBER_MIN = 0.0;
  static constexpr double ABS_SPEED_SAMPLE_MEMBER_MAX = 300.0;
  static constexpr double TEMP_SAMPLE_MEMBER_MIN = -5.0;
  static constexpr double TEMP_SAMPLE_MEMBER_MAX = 40.0;

public:
  bool subscribe() override;
  void aggregate(void);

private:
  static void aanderaSubCallback(uint64_t node_id, const char *topic, uint16_t topic_len,
                                 const uint8_t *data, uint16_t data_len, uint8_t type,
                                 uint8_t version);

private:
  static constexpr char subtag[] = "/sofar/aanderaa";
} Aanderaa_t;

Aanderaa_t* createAanderaaSub(uint64_t node_id, uint32_t current_agg_period_ms, uint32_t averager_max_samples);
