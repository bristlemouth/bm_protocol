#pragma once

#include "FreeRTOS.h"
#include "queue.h"
#include "configuration.h"

typedef enum {
  REPORT_BUILDER_INCREMENT_SAMPLE_COUNT,
  REPORT_BUILDER_SAMPLE_MESSAGE,
} report_builder_message_e;

typedef struct {
  report_builder_message_e message_type;
  uint64_t node_id;
  uint8_t sensor_type;
  uint8_t *sensor_data;
} report_builder_queue_item_t;

void reportBuilderInit(cfg::Configuration* sys_cfg);