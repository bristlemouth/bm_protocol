/**
 * @file aanderaaConductivitySensor.cpp
 * @brief Implementation of Aanderaa Conductivity Sensor Bridge Driver
 *
 * This file implements the bridge-side functionality for handling Aanderaa
 * conductivity sensor data. It manages CBOR message reception, statistical
 * aggregation, and integration with the bridge reporting system.
 */
#include "aanderaaConductivitySensor.h"
#include "FreeRTOS.h"
#include "bm_config.h"
#include "bm_os.h"
#include "bridgeLog.h"
#include "cbor.h"
#include "pubsub.h"
#include "semphr.h"
#include "sensorController.h"
#include "spotter.h"
#include "util.h"
#include <new>

/**
 * @brief Constructor - initializes all samplers and counters
 */
AanderaaConductivitySensor::AanderaaConductivitySensor(uint64_t conductivity_node_id,
                                                       uint32_t agg_period_ms,
                                                       uint32_t averager_max_samples) {

  for (auto &sampler : samplers.data) {
    sampler.initBuffer(averager_max_samples);
  }

  reading_count = 0;
  last_timestamp = 0;
  node_position = 0;

  _mutex = xSemaphoreCreateMutex();
  configASSERT(_mutex);

  node_id = conductivity_node_id;
  type = SENSOR_TYPE_AANDERAA_CONDUCTIVITY;
  next = NULL;
  current_agg_period_ms = agg_period_ms;
}

/**
 * @brief Subscribe to conductivity sensor data topic
 * @details This function is called when the report builder is subscribing to the sensor data topic
 *
 * @return true if subscription successful, false otherwise
 */
bool AanderaaConductivitySensor::subscribe(void) {
  bool rval = false;
  char *sub = static_cast<char *>(bm_malloc(BM_TOPIC_MAX_LEN));
  configASSERT(sub);
  int topic_strlen =
      snprintf(sub, BM_TOPIC_MAX_LEN, "sensor/%016" PRIx64 "%s", node_id, subtag);
  if (topic_strlen > 0) {
    rval = bm_sub_wl(sub, topic_strlen, sub_callback) == BmOK;
  }
  bm_free(sub);
  return rval;
}

/**
 * @brief Aggregate collected sensor data and submit to report builder
 *
 * @details This function calculates statistical means for all sensor parameters
 * and submits the aggregated data to the bridge's reporting system.
 * Only aggregates if minimum number of readings have been collected.
 */
void AanderaaConductivitySensor::aggregate(void) {
  if (xSemaphoreTake(_mutex, portMAX_DELAY)) {

    // Initialize aggregation structure with NaN values (invalid data indicator)
    AanderaaConductivityAggregations conductivity_aggs = {};

    // Only calculate means if we have sufficient data points
    if (samplers[SamplerType::Conductivity_ms_cm].getNumSamples() >
        MIN_READINGS_FOR_AGGREGATION) {

      // Update conductivity_aggs from AverageSamplers
      for (const auto &f : kSamplerMap) {
        (conductivity_aggs.*(f.aggr_ptr)) = samplers[f.type].getMean();
      }
      conductivity_aggs.salinity_std_dev = samplers[SamplerType::Salinity_psu].getStd();

      if (conductivity_aggs.salinity_std_dev > SALINITY_STD_DEV_MAX) {
        conductivity_aggs.salinity_std_dev = HUGE_VAL;
      }

      conductivity_aggs.reading_count = reading_count;
    }
    send_spotter_log_aggregate(conductivity_aggs);

    // Submit aggregated data to bridge reporting system
    reportBuilderAddToQueue(
        node_id, SENSOR_TYPE_AANDERAA_CONDUCTIVITY, static_cast<void *>(&conductivity_aggs),
        sizeof(AanderaaConductivityAggregations), REPORT_BUILDER_SAMPLE_MESSAGE);

    for (auto &sampler : samplers.data) {
      sampler.clear();
    }

    reading_count = 0;
    xSemaphoreGive(_mutex);
  } else {
    bm_debug("Failed to get the Aanderaa Conductivity mutex while trying to aggregate\n");
  }
}

/**
 * @brief Get sample member parameters for encoding sensor data
 *
 * @details This function is called when the report builder is setting up the
 *          sample member parameters
 *
 * @param sensor_data Pointer to sensor data
 * @param sample_index Index of sample to encode
 * @param sampleMemberParams
 */
void AanderaaConductivitySensor::get_report_builder_sample_params(void *sensor_data,
                                                                  uint32_t sample_index,
                                                                  ReportParams &report_params) {
  AanderaaConductivityAggregations &aanderaa_conductivity_sample =
      (static_cast<AanderaaConductivityAggregations *>(sensor_data))[sample_index];

  SampleMemberParams *params = report_params.sample_members;
  uint32_t sample_member_count = 0;

  // sampleMemberParams is allocated as a fixed-size array in report_params_t (stack-based, no heap allocation)
  params[sample_member_count++] = {
      .sample_member = &aanderaa_conductivity_sample.conductivity_mean_ms_cm,
      .sample_member_encoder_cb = encode_double_sample_member,
      .size = 0,
  };
  params[sample_member_count++] = {
      .sample_member = &aanderaa_conductivity_sample.temperature_mean_deg_c,
      .sample_member_encoder_cb = encode_double_sample_member,
      .size = 0,
  };
  params[sample_member_count++] = {
      .sample_member = &aanderaa_conductivity_sample.salinity_mean_psu,
      .sample_member_encoder_cb = encode_double_sample_member,
      .size = 0,
  };
  params[sample_member_count++] = {
      .sample_member = &aanderaa_conductivity_sample.salinity_std_dev,
      .sample_member_encoder_cb = encode_double_sample_member,
      .size = 0,
  };
  params[sample_member_count++] = {
      .sample_member = &aanderaa_conductivity_sample.water_density_mean_kg_m3,
      .sample_member_encoder_cb = encode_double_sample_member,
      .size = 0,
  };
  params[sample_member_count++] = {
      .sample_member = &aanderaa_conductivity_sample.sound_speed_mean_m_s,
      .sample_member_encoder_cb = encode_double_sample_member,
      .size = 0,
  };
  params[sample_member_count++] = {
      .sample_member = &aanderaa_conductivity_sample.depth_mean_m,
      .sample_member_encoder_cb = encode_double_sample_member,
      .size = 0,
  };

  configASSERT_EXTRA(sample_member_count == report_params.num_sample_members,
                     "ERROR: Incorrect number of sample members for Aanderaa Conductivity\n");
}

/**
 * @brief Get report parameters for encoding sensor data in report builder
 *
 * @details This function is called when the report builder is setting up the report parameters
 *
 * @param context Encoder context
 * @param sensor_data Pointer to sensor data
 * @param sample_index Index of sample to encode
 *
 * @return Report parameters
 */
ReportParams
AanderaaConductivitySensor::get_report_params(sensor_report_encoder_context_t &context,
                                              void *sensor_data, uint32_t sample_index) {

  static SampleMemberParams sample_members[AANDERAA_CONDUCTIVITY_NUM_SAMPLE_MEMBERS];

  ReportParams params = {
      .context = context,
      .sensor_data = sensor_data,
      .sample_index = sample_index,
      .fail_text = "Failed to open aanderaa_conductivity sample in addSamplesToReport\n",
      .sample_type = "bm_aanderaa_conductivity_v0",
      .num_sample_members = AANDERAA_CONDUCTIVITY_NUM_SAMPLE_MEMBERS,
      .sample_members = sample_members,
  };

  get_report_builder_sample_params(sensor_data, sample_index, params);

  return params;
}

/**
 * @brief Helper function to set up conductivity sensor data pointers.
 *
 * @details This function is called when the report builder is setting up the sensor data pointers
 *
 * @param element Pointer to the element in the linked list.
 * @param nan_sample Pointer to store the NAN sample reference.
 * @param dst Pointer to store the destination data reference.
 */
void AanderaaConductivitySensor::setup_sensor_pointers(report_builder_element_t *element,
                                                       const void **nan_sample, void **dst) {
  *nan_sample = &AanderaaConductivitySensor::aanderaa_conductivity_NAN_AGG;
  *dst = &(static_cast<AanderaaConductivityAggregations *>(
      element->sensor_data))[element->sample_counter];
}

/**
 * @brief Format and send individual sensor reading to spotter log via AbstractSensor
 *
 * @param m Sensor data
 *
 * @return BmErr error code
 */
BmErr AanderaaConductivitySensor::send_spotter_log_individual(
    const AanderaaConductivityMsg::Data &m) {
  BmErr err = AbstractSensor::send_spotter_log_individual(
      "aanderaa_conductivity", m.header,
      DEFAULT_AANDERAA_CONDUCTIVITY_READING_PERIOD_MS + 1000U,
      "%.4f,"   // conductivity_ms_cm
      "%.3f,"   // temperature_deg_c
      "%.3f,"   // salinity_psu
      "%.3f,"   // water_density_kg_m3
      "%.3f,"   // sound_speed_m_s
      "%.3f\n", // depth_m
      m.conductivity_ms_cm, m.temperature_deg_c, m.salinity_psu, m.water_density_kg_m3,
      m.sound_speed_m_s, static_cast<double>(m.depth_m));

  if (err != BmOK) {
    bm_debug("ERROR: Failed to send Aanderaa Conductivity individual log to spotter, err: %d\n",
             err);
  }

  return err;
}

/**
 * @brief Format and send aggregated sensor data to spotter log via AbstractSensor
 *
 * @param agg Aggregated data
 *
 * @return BmErr error code
 */
BmErr AanderaaConductivitySensor::send_spotter_log_aggregate(
    const AanderaaConductivityAggregations &agg) {

  BmErr err = AbstractSensor::send_spotter_log_aggregate(
      "aanderaa_conductivity", agg.reading_count,
      "%.4f,"   // conductivity_mean_ms_cm
      "%.3f,"   // temperature_mean_deg_c
      "%.3f,"   // salinity_mean_psu
      "%.4f,"   // salinity_std_dev
      "%.3f,"   // water_density_mean_kg_m3
      "%.3f,"   // sound_speed_mean_m_s
      "%.3f\n", // depth_mean_m
      agg.conductivity_mean_ms_cm, agg.temperature_mean_deg_c, agg.salinity_mean_psu,
      agg.salinity_std_dev, agg.water_density_mean_kg_m3, agg.sound_speed_mean_m_s,
      agg.depth_mean_m);

  if (err != BmOK) {
    bm_debug("ERROR: Failed to send PME DO aggregate log to spotter, err: %d\n", err);
  }

  return err;
}

/**
 * @brief Static callback for handling incoming conductivity sensor data
 *
 * @details This function is called when CBOR-encoded conductivity sensor data is received
 * via the pub/sub system. It decodes the data and adds samples to the appropriate
 * sensor's statistical aggregators.
 *
 * @param node_id Source node ID
 * @param topic MQTT topic string
 * @param topic_len Length of topic string
 * @param data CBOR-encoded sensor data
 * @param data_len Length of data buffer
 * @param type Message type (unused)
 * @param version Message version (unused)
 */
void AanderaaConductivitySensor::sub_callback(uint64_t node_id, const char *topic,
                                              uint16_t topic_len, const uint8_t *data,
                                              uint16_t data_len, uint8_t type,
                                              uint8_t version) {
  (void)type;    // Unused parameter
  (void)version; // Unused parameter

  bm_debug("Aanderaa Conductivity data received from node %016" PRIx64 ", on topic: %.*s\n",
           node_id, topic_len, topic);

  // Find the sensor instance for this node
  AanderaaConductivity_t *conductivity_sensor = static_cast<AanderaaConductivity_t *>(
      sensorControllerFindSensorById(node_id, SENSOR_TYPE_AANDERAA_CONDUCTIVITY));

  if (conductivity_sensor && conductivity_sensor->type == SENSOR_TYPE_AANDERAA_CONDUCTIVITY) {
    if (xSemaphoreTake(conductivity_sensor->_mutex, portMAX_DELAY)) {
      AanderaaConductivityMsg::Data composite_cbor_msg;
      // Decode CBOR message
      if (AanderaaConductivityMsg::decode(composite_cbor_msg, data, data_len) == CborNoError) {
        // Add sensor readings to statistical samplers for aggregation
        for (const auto &f : kSamplerMap) {
          // This is a bit "extra" because AanderaaConductivityMsg::Data defines depth_m as float, but others are double
          // std::visit is used to dereference whichever type of member pointer is stored.
          // Each value is cast to `double` before being passed into the sampler, so all samplers
          // consistently operate on `double` input
          const double v = std::visit(
              [&](auto ptr) {
                return static_cast<double>(composite_cbor_msg.*
                                           ptr); // depth_m (float) becomes double here
              },
              f.cbor_ptr);

          // Skip invalid values -- can may be unset, uninitialized, or explicitly encoded as “NaN” (not-a-number) to mean “no reading”.
          if (!std::isnan(v)) {
            conductivity_sensor->samplers[f.type].addSample(v);
          }
        }
        conductivity_sensor->reading_count++;

        conductivity_sensor->send_spotter_log_individual(composite_cbor_msg);

      } else {
        bm_debug("ERROR: Failed to decode CBOR message from node %016" PRIx64 "\n", node_id);
      }
      xSemaphoreGive(conductivity_sensor->_mutex);
    } else {
      bm_debug("Failed to take mutex for Aanderaa Conductivity after getting a new reading\n");
    }
  }
}

/**
 * @brief Factory function to create and configure a conductivity sensor instance
 *
 * @details Allocates memory for a new conductivity sensor, initializes all statistical
 * samplers, and configures the sensor for the specified node and parameters.
 *
 * @param node_id Target node ID to monitor
 * @param agg_period_ms Aggregation period in milliseconds
 * @param averager_max_samples Maximum samples for averaging
 * @return Pointer to configured sensor instance, or nullptr on failure
 */
AanderaaConductivity_t *createAanderaaConductivitySub(uint64_t node_id, uint32_t agg_period_ms,
                                                      uint32_t averager_max_samples) {
  // Allocate memory and construct sensor instance
  AanderaaConductivity_t *new_sub =
      static_cast<AanderaaConductivity_t *>(bm_malloc(sizeof(AanderaaConductivity_t)));
  configASSERT(new_sub);
  new_sub = new (new_sub) AanderaaConductivity_t(node_id, agg_period_ms, averager_max_samples);
  configASSERT(new_sub);

  // Moved everything else to the constructor

  return new_sub;
}
