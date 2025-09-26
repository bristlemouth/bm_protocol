#pragma once

#include "abstractSensor.h"
#include "cbor_sensor_report_encoder.h"

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

/**
 * @brief Parameters for encoding individual sample members (fields) within sensor data
 *
 * Used to pass sample member data and encoding information to the CBOR functions.
 * Each sensor reading typically contains multiple sample members (e.g., temperature, conductivity)
 * that need to be encoded with their appropriate encoder callbacks.
 *
 */
typedef struct {
    /// Pointer to the data value to be encoded
    void *sample_member;
    /// Callback function to encode the sample member data
    sample_encoder_cb sample_member_encoder_cb;
    /// Size of the data
    uint32_t size;
} SampleMemberParams;


/// @brief Maximum number of sample members across all sensor types
/// Based on max across all sensors (currently 8, with some headroom)
#define MAX_SAMPLE_MEMBERS 10
/**
 * @brief Parameters for building complete sensor reports
 *
 * Contains information needed to construct a sensor report including
 * context, sensor data, and metadata for CBOR encoding. Used by the report
 * builder system to process sensor data in a structured way.
 */
 // cppcheck-suppress unusedStructMember
 typedef struct {
  /// Context for CBOR encoding
  sensor_report_encoder_context_t &context;
  /// Pointer to the sensor data to be encoded
  void *sensor_data;
  /// Index of the sample to be encoded
  uint32_t sample_index;
  /// Failure text for logging
  const char *fail_text;
  /// Sample type identifier
  const char *sample_type;
  /// Number of sample members (fields) in the sensor data
  uint32_t num_sample_members;
  /// Fixed-size array of sample member parameters (no heap allocation)
  SampleMemberParams sample_members[MAX_SAMPLE_MEMBERS];

} ReportParams;

void reportBuilderInit(void);

void reportBuilderAddToQueue(uint64_t node_id, uint8_t sensor_type, void *sensor_data,
                             uint32_t sensor_data_size, report_builder_message_e msg_type);
uint8_t *report_builder_alloc_last_network_config(uint32_t &network_crc32,
                                                  uint32_t &cbor_config_size);
uint32_t report_builder_get_samples_per_report(void);

bool report_builder_get_transmit_aggregations(void);

CborError encode_buffer_sample_member(CborEncoder &sample_array, void *sample_member,
                                      uint32_t size);

CborError encode_double_sample_member(CborEncoder &sample_array, void *sample_member,
                                      uint32_t size);

CborError encode_uint_sample_member(CborEncoder &sample_array, void *sample_member,
                                    uint32_t size);
