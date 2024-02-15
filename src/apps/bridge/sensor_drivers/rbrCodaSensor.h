#pragma once
#include "FreeRTOS.h"
#include "abstractSensor.h"
#include "avgSampler.h"
#include "bm_rbr_data_msg.h"
#include "sensorController.h"

#include <stdint.h>
#include <stdlib.h>

#define RBR_CODA_NUM_SAMPLE_MEMBERS 3

typedef struct rbr_coda_aggregations_s {
  double temp_mean_deg_c;
  double pressure_mean_deci_bar;
  double pressure_stdev_deci_bar;
  uint32_t reading_count;
  BmRbrDataMsg::SensorType_t sensor_type;
} rbr_coda_aggregations_t;

typedef struct RbrCodaSensor : public AbstractSensor {
  uint32_t rbr_coda_agg_period_ms;
  AveragingSampler temp_deg_c;
  AveragingSampler pressure_ubar;
  uint32_t reading_count;
  BmRbrDataMsg::SensorType_t latest_sensor_type;

  // Extra sample padding to account for timing slop. Calculated as the sample frequency + 2 minutes bridge on period + some extra slop.
  // 2 minutes is the minimum bridge on period and the soft by default is sampling at 2Hz. So 2*120 + 30 = 270.
  static constexpr uint32_t N_SAMPLES_PAD = 270;
  static constexpr uint8_t MIN_READINGS_FOR_AGGREGATION = 3;
  static constexpr double TEMP_SAMPLE_MEMBER_MIN = -5;
  static constexpr double TEMP_SAMPLE_MEMBER_MAX = 35;
  static constexpr double PRESSURE_SAMPLE_MEMBER_MIN = 5;
  static constexpr double PRESSURE_SAMPLE_MEMBER_MAX = 85;
  static constexpr double PRESSURE_STDEV_SAMPLE_MEMBER_MIN = 0;
  static constexpr double PRESSURE_STDEV_SAMPLE_MEMBER_MAX = 40;

public:
  bool subscribe() override;
  void aggregate(void);

private:
  static void rbrCodaSubCallback(uint64_t node_id, const char *topic, uint16_t topic_len,
                                 const uint8_t *data, uint16_t data_len, uint8_t type,
                                 uint8_t version);

private:
  static constexpr char subtag[] = "/bm_rbr_data";
} RbrCoda_t;

RbrCoda_t *createRbrCodaSub(uint64_t node_id, uint32_t rbr_coda_agg_period_ms,
                            uint32_t averager_max_samples);