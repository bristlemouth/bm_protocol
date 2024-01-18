#pragma once

#include "FreeRTOS.h"
#include "queue.h"
#include "configuration.h"
#include "aanderaaSensor.h"
#include "softSensor.h"

typedef enum {
  REPORT_BUILDER_INCREMENT_SAMPLE_COUNT,
  REPORT_BUILDER_SAMPLE_MESSAGE,
  REPORT_BUILDER_CHECK_CRC,
} report_builder_message_e;

typedef enum : uint8_t{
  UNKNOWN_SENSOR_TYPE = 0,
  AANDERAA_SENSOR_TYPE = 1,
  SOFT_SENSOR_TYPE = 2,
} report_builder_sensor_type_e;

typedef struct {
  report_builder_message_e message_type;
  uint64_t node_id;
  uint8_t sensor_type;
  void *sensor_data;
  uint32_t sensor_data_size;
} report_builder_queue_item_t;

void reportBuilderInit(cfg::Configuration* sys_cfg);
void reportBuilderAddToQueue(uint64_t node_id, uint8_t sensor_type, void *sensor_data, uint32_t sensor_data_size, report_builder_message_e msg_type);
