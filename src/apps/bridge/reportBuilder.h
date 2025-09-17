#pragma once

#include "abstractSensor.h"
#include "cbor_sensor_report_encoder.h"
#include <vector>

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
 * Each sensor reading typically contains multiple fields (e.g., temperature, conductivity)
 * that need to be encoded with their appropriate encoder callbacks.
 *
 * TODO: Move to sensor-specific classes when sensor registry API is implemented.
 */
typedef struct {
    /// Pointer to the data value to be encoded
    void *sampleMember;
    /// Callback function to encode the sample member data
    sample_encoder_cb sampleMemberEncoderCb;
    /// Size of the data
    uint32_t size;
} sample_member_params_t;


/**
 * @brief Parameters for building complete sensor reports
 *
 * Contains information needed to construct a sensor report including
 * context, sensor data, and metadata for CBOR encoding. Used by the report
 * builder system to process sensor data in a structured way.
 */
 typedef struct {
  /// Context for CBOR encoding
  sensor_report_encoder_context_t &context;
  /// Pointer to the sensor data to be encoded
  void *sensor_data;
  /// Index of the sample to be encoded
  uint32_t sample_index;
  /// Failure text for logging
  const char *failText;
  /// Sample type identifier
  const char *sampleType;
  /// Number of sample members (fields) in the sensor data
  uint32_t numSampleMembers;
  /// Vector of sample member parameters
  std::vector<sample_member_params_t> sampleMembers;

} report_params_t;

void reportBuilderInit(void);

void reportBuilderAddToQueue(uint64_t node_id, uint8_t sensor_type, void *sensor_data,
                             uint32_t sensor_data_size, report_builder_message_e msg_type);
uint8_t *report_builder_alloc_last_network_config(uint32_t &network_crc32,
                                                  uint32_t &cbor_config_size);
uint32_t report_builder_get_samples_per_report(void);

bool report_builder_get_transmit_aggregations(void);

/**
 * @brief Open a new sample in the sensor report encoder
 *
 * Initializes a new sample entry in the CBOR report with the specified
 * number of sample members and sample type identifier.
 *
 * @param params Report parameters containing context and metadata
 * @return true if successful, false on encoding error
 */
bool report_builder_open_sample(report_params_t &params);

/**
 * @brief Add a sample member (field) to the current sample
 *
 * Encodes a single data field from the sensor reading into the CBOR report
 * using the appropriate encoder callback. This function handles the encoding
 * of individual sensor values like temperature, conductivity, etc.
 *
 * @param params Report parameters containing context and metadata
 * @param s Sample member parameters containing data pointer, encoder, and context
 * @return true if successful, false on encoding error
 */
bool report_builder_add_sample_member(report_params_t &params, sample_member_params_t &s);

/**
 * @brief Close the current sample in the sensor report encoder
 *
 * Finalizes the current sample entry in the CBOR report. Must be called
 * after all sample members have been added.
 *
 * @param params Report parameters containing context
 * @return true if successful, false on encoding error
 */
bool report_builder_close_sample(report_params_t &params);

/**
 * @brief Add all sample members from a report to the encoder
 *
 * Convenience function that opens a sample, adds all sample members from
 * the report parameters, and closes the sample. This is the main function
 * sensors should use to add their data to reports.
 *
 * @param params Report parameters containing sample members and context
 * @return true if successful, false on encoding error
 */
bool report_builder_add_samples(report_params_t &params);

CborError encode_buffer_sample_member(CborEncoder &sample_array, void *sample_member,
                                      uint32_t size);

CborError encode_double_sample_member(CborEncoder &sample_array, void *sample_member,
                                      uint32_t size);

CborError encode_uint_sample_member(CborEncoder &sample_array, void *sample_member,
                                    uint32_t size);
