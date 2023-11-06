#pragma once
#include "cbor.h"
#include <stdlib.h>

typedef struct sensor_report_encoder_context {
  uint8_t *buffer;
  CborEncoder encoder;
  CborEncoder report_array;
  CborEncoder sensor_array;
  CborEncoder sample_array;
} sensor_report_encoder_context_t;

typedef CborError (*sample_encoder_cb)(CborEncoder &sample_array, void *sensor_data);

CborError sensor_report_encoder_open_report(uint8_t *cbor_buffer, size_t cbor_buffer_len,
                                            uint8_t num_sensors,
                                            sensor_report_encoder_context_t &context);

CborError sensor_report_encoder_close_report(sensor_report_encoder_context_t &context);

CborError sensor_report_encoder_open_sensor(sensor_report_encoder_context_t &context,
                                            size_t num_sensors);

CborError sensor_report_encoder_close_sensor(sensor_report_encoder_context_t &context);

CborError sensor_report_encoder_open_sample(sensor_report_encoder_context_t &context,
                                            size_t num_samples);

CborError sensor_report_encoder_add_sample(sensor_report_encoder_context_t &context,
                                           sample_encoder_cb sample_encoder_cb,
                                           void *sensor_data);

CborError sensor_report_encoder_close_sample(sensor_report_encoder_context_t &context);

size_t sensor_report_encoder_get_report_size_bytes(sensor_report_encoder_context_t &context);
