#pragma once
#include "FreeRTOS.h"
#include "semphr.h"
#include "sensor_header_msg.h"
#include "util.h"
#include <stdint.h>

typedef enum abstractSensorType : uint8_t {
  SENSOR_TYPE_UNKNOWN = 0,
  SENSOR_TYPE_AANDERAA = 1,
  SENSOR_TYPE_SOFT = 2,
  SENSOR_TYPE_RBR_CODA = 3,
  SENSOR_TYPE_SEAPOINT_TURBIDITY = 4,
  SENSOR_TYPE_BOREALIS = 5,
  SENSOR_TYPE_PME_DO = 6,
  SENSOR_TYPE_PME_WIPE = 7,
} abstractSensorType_e;

struct AbstractSensor {
  uint64_t node_id;
  AbstractSensor *next;
  SemaphoreHandle_t _mutex;
  abstractSensorType_e type;
  uint32_t m_reading_period_ms;
  uint32_t m_sample_duration_ms;
  bool m_subsample_enabled;
  uint32_t m_subsample_interval_ms;
  uint32_t m_subsample_duration_ms;

private:
  struct {
    uint32_t last_timestamp_ms;
    bool first_time;
    int8_t node_position;
  } m_position;

public:
  AbstractSensor();
  virtual bool subscribe() = 0;
  int8_t update_node_position(uint32_t max_reading_period_ms);
  BmErr send_spotter_log_individual(const char *app_name, SensorHeaderMsg::Data header,
                                    uint32_t sensor_max_reading_ms, const char *fmt, ...);
  BmErr send_spotter_log_aggregate(const char *app_name, uint32_t reading_count,
                                   const char *fmt, ...);
};
