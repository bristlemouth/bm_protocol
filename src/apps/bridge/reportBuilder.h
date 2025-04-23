#pragma once

#include "FreeRTOS.h"
#include "aanderaaSensor.h"
#include "configuration.h"
#include "queue.h"
#include "softSensor.h"

typedef enum {
  REPORT_BUILDER_INCREMENT_SAMPLE_COUNT,
  REPORT_BUILDER_SAMPLE_MESSAGE,
  REPORT_BUILDER_CHECK_CRC,
} report_builder_message_e;

typedef struct {
  report_builder_message_e message_type;
  uint64_t node_id;
  uint8_t sensor_type;
  void *sensor_data;
  uint32_t sensor_data_size;
} report_builder_queue_item_t;

void reportBuilderInit(void);
void reportBuilderAddToQueue(uint64_t node_id, uint8_t sensor_type, void *sensor_data,
                             uint32_t sensor_data_size, report_builder_message_e msg_type);
CborError encode_string_sample_member(CborEncoder &sample_array, void *sample_member,
                                      uint32_t size);
CborError encode_double_sample_member(CborEncoder &sample_array, void *sample_member,
                                      uint32_t size);
CborError encode_uint8_sample_member(CborEncoder &sample_array, void *sample_member,
                                     uint32_t size);
CborError encode_uint32_sample_member(CborEncoder &sample_array, void *sample_member,
                                      uint32_t size);
uint8_t *report_builder_alloc_last_network_config(uint32_t &network_crc32,
                                                  uint32_t &cbor_config_size);
