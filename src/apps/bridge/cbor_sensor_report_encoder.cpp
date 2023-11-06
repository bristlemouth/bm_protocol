#include "cbor_sensor_report_encoder.h"

/*!
 * @brief Open a sensor report for encoding. Must be called before any other encoding functions.
 * @param[in] cbor_buffer The buffer to encode into.
 * @param[in] cbor_buffer_len The length of the buffer.
 * @param[in] num_sensors The number of sensors in the report.
 * @param[in] context The context to use for the entire encoding.
 * This context should not persist in between building reports.
 * @return Cbor error code.
 */
CborError sensor_report_encoder_open_report(uint8_t *cbor_buffer, size_t cbor_buffer_len,
                                            uint8_t num_sensors,
                                            sensor_report_encoder_context_t &context) {
  context.buffer = cbor_buffer;
  cbor_encoder_init(&context.encoder, context.buffer, cbor_buffer_len, 0);
  return cbor_encoder_create_array(&context.encoder, &context.report_array, num_sensors);
}

/*!
 * @brief Close a sensor report for encoding. Must be called when done building report.
 * @param[in] context The encoding context.
 * @return Cbor error code.
 */
CborError sensor_report_encoder_close_report(sensor_report_encoder_context_t &context) {
  return cbor_encoder_close_container(&context.encoder, &context.report_array);
}

/*!
 * @brief Open a sensor array for encoding.
 * @param[in] context The encoding context.
 * @param[in] num_sensors The number of samples in the sensor.
 * @return Cbor error code.
 */
CborError sensor_report_encoder_open_sensor(sensor_report_encoder_context_t &context,
                                            size_t num_sensors) {
  return cbor_encoder_create_array(&context.report_array, &context.sensor_array, num_sensors);
}

/*!
 * @brief Close a sensor array for encoding. Must be called when all samples for a sensor have been added.
 * @param[in] context The encoding context.
 * @return Cbor error code.
 */
CborError sensor_report_encoder_close_sensor(sensor_report_encoder_context_t &context) {
  return cbor_encoder_close_container(&context.report_array, &context.sensor_array);
}

/*!
 * @brief Open a sample array for encoding.
 * @param[in] context The encoding context.
 * @param[in] num_samples The number of samples in the sensor.
 * @return Cbor error code.
 */
CborError sensor_report_encoder_open_sample(sensor_report_encoder_context_t &context,
                                            size_t num_samples) {
  return cbor_encoder_create_array(&context.sensor_array, &context.sample_array, num_samples);
}

/*!
 * @brief Add a sample to the sample array.
 * @param[in] context The encoding context.
 * @param[in] sample_encoder_cb The callback to encode the sample data.
 * @param[in] sensor_data The sensor data to encode.
 * @return Cbor error code.
 */
CborError sensor_report_encoder_add_sample(sensor_report_encoder_context_t &context,
                                           sample_encoder_cb sample_encoder_cb,
                                           void *sensor_data) {
  return sample_encoder_cb(context.sample_array, sensor_data);
}

/*!
 * @brief Close a sample array for encoding. Must be called when all samples have been added.
 * @param[in] context The encoding context.
 * @return Cbor error code.
 */
CborError sensor_report_encoder_close_sample(sensor_report_encoder_context_t &context) {
  return cbor_encoder_close_container(&context.sensor_array, &context.sample_array);
}

/*!
 * @brief Get the size of the encoded report.
 * @param[in] context The encoding context.
 * @return The size of the encoded report.
 */
size_t sensor_report_encoder_get_report_size_bytes(sensor_report_encoder_context_t &context) {
  return cbor_encoder_get_buffer_size(&context.encoder, context.buffer);
}
