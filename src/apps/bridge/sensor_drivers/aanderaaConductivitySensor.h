/**
 * @file aanderaaConductivitySensor.h
 * @brief Aanderaa Conductivity Sensor Bridge Driver
 *
 * This file implements the bridge-side driver for Aanderaa conductivity sensors.
 * The driver handles CBOR message reception, data aggregation, and integration
 * with the bridge's sensor management system.
 *
 * Sensor Data:
 * - Conductivity (mS/cm)
 * - Temperature (°C)
 * - Salinity (PSU)
 * - Water density (kg/m³)
 * - Sound speed (m/s)
 * - Depth (m)
 *
 * Topic: sensor/{node_id}/sofar/aanderaa_conductivity_data
 * Message Format: AanderaaConductivityMsg (CBOR)
 * Sensor Type: SENSOR_TYPE_AANDERAA_CONDUCTIVITY (8)
 */

#pragma once
#include "abstractSensor.h"
#include "aanderaa_conductivity_msg.h"
#include "avgSampler.h"
#include "sensorController.h"
#include "reportBuilder.h"
#include "reportBuilderList.h"
#include <array>
#include <cmath>
#include <stdint.h>
#include <stdlib.h>
#include <variant>


/**
 * @brief Aggregated conductivity sensor data structure
 *
 * Contains the averaged/aggregated values from multiple conductivity sensor readings.
 * This structure is used for storing and transmitting aggregated sensor data
 * in the bridge's report system.
 */
typedef struct {
  /// Mean conductivity in mS/cm
  double conductivity_mean_ms_cm = NAN;
  /// Mean temperature in degrees Celsius
  double temperature_mean_deg_c = NAN;
  /// Mean salinity in Practical Salinity Units
  double salinity_mean_psu = NAN;
  /// Mean water density in kg/m³
  double water_density_mean_kg_m3 = NAN;
  /// Mean sound speed in m/s
  double sound_speed_mean_m_s = NAN;
  /// Mean depth in meters
  double depth_mean_m = NAN;
  /// Number of readings used in aggregation
  uint32_t reading_count = 0;
} AanderaaConductivityAggregations;

/**
 * @brief Aanderaa Conductivity Sensor Bridge Driver Class
 *
 * Handles reception and aggregation of conductivity sensor data from remote nodes.
 * Inherits from AbstractSensor to integrate with the bridge's sensor management system.
 *
 * Features:
 * - CBOR message reception via pub/sub
 * - Statistical aggregation of sensor readings
 * - Integration with bridge reporting system
 * - Configurable aggregation periods
 */
typedef struct AanderaaConductivitySensor : public AbstractSensor {

private:
  /// Aggregation period in milliseconds
  uint32_t current_agg_period_ms;

  /// Enum identifying each type of sampler we track. The last entry "Count"
  /// is used to size arrays (it is not an actual sampler).
  enum class SamplerType {
    Conductivity_ms_cm,
    Temperature_deg_c,
    Salinity_psu,
    Water_density_kg_m3,
    Sound_speed_m_s,
    Depth_m,
    Count
  };

  
  // Container for all samplers, indexed by SamplerType.
  // Provides operator[] overloads so we can write samplers[SamplerType::X]
  // instead of manually casting enum values to array indices.
  struct SamplerSet {
      std::array<AveragingSampler, static_cast<size_t>(SamplerType::Count)> data{};

      AveragingSampler& operator[](SamplerType type) {
          return data[static_cast<size_t>(type)];
      }
      const AveragingSampler& operator[](SamplerType type) const {
          return data[static_cast<size_t>(type)];
      }
  };

  // the actual collection of samplers
  SamplerSet samplers;

  // Mapping table from a SamplerType to the corresponding "mean" field
  // inside AanderaaConductivityAggregations. Used when copying sampler
  // statistics back into the aggregation struct.
  struct MapSamplerToAggregation {
    SamplerType type;
    double AanderaaConductivityAggregations::* mean_ptr;
  };

  static constexpr MapSamplerToAggregation kMapSamplerToAggregation[] = {
    {SamplerType::Conductivity_ms_cm,  &AanderaaConductivityAggregations::conductivity_mean_ms_cm},
    {SamplerType::Temperature_deg_c,   &AanderaaConductivityAggregations::temperature_mean_deg_c},
    {SamplerType::Salinity_psu,        &AanderaaConductivityAggregations::salinity_mean_psu},
    {SamplerType::Water_density_kg_m3, &AanderaaConductivityAggregations::water_density_mean_kg_m3},
    {SamplerType::Sound_speed_m_s,     &AanderaaConductivityAggregations::sound_speed_mean_m_s},
    {SamplerType::Depth_m,             &AanderaaConductivityAggregations::depth_mean_m},
  };

  // Convenience aliases for pointer-to-member types into the CBOR message:
  // DPtr points to a double field, FPtr points to a float field.
  using DPtr = double AanderaaConductivityMsg::Data::*;
  using FPtr = float  AanderaaConductivityMsg::Data::*;  // for depth_m

  // Mapping table from a SamplerType to the corresponding field in the
  // AanderaaConductivityMsg::Data struct (decoded from CBOR).
  // Because some fields are double and one (depth) is float, we store them
  // in a std::variant<DPtr,FPtr>. Later, std::visit is used to extract the
  // field, promoting float to double for consistent sampler input.
  struct MapSamplerToCbor {
      SamplerType type;
      std::variant<DPtr, FPtr> value_ptr;
  };

  static constexpr MapSamplerToCbor kMapSamplerToCbor[] = {
      {SamplerType::Conductivity_ms_cm,  &AanderaaConductivityMsg::Data::conductivity_ms_cm},
      {SamplerType::Temperature_deg_c,   &AanderaaConductivityMsg::Data::temperature_deg_c},
      {SamplerType::Salinity_psu,      &AanderaaConductivityMsg::Data::salinity_psu},
      {SamplerType::Water_density_kg_m3,  &AanderaaConductivityMsg::Data::water_density_kg_m3},
      {SamplerType::Sound_speed_m_s,    &AanderaaConductivityMsg::Data::sound_speed_m_s},
      {SamplerType::Depth_m,         &AanderaaConductivityMsg::Data::depth_m}, // cast to double below
  };

  /// Total number of readings received
  uint32_t reading_count;
  /// Position of this node in sensor array
  int8_t node_position;
  /// Timestamp of last received reading
  uint32_t last_timestamp;
  public:

  /** @brief Number of sensor parameters measured by the Aanderaa conductivity sensor */
  static constexpr uint32_t AANDERAA_CONDUCTIVITY_NUM_SAMPLE_MEMBERS = 6;

  /**
   * @brief Sample buffer padding for timing variations
   *
   * Calculated as: sample_frequency + bridge_on_period + safety_margin
   * Default: 1Hz * 120s + 30s = 150 samples
   * Accounts for timing slop between sensor sampling and bridge aggregation periods.
   */
  static constexpr uint32_t N_SAMPLES_PAD = 150;

  /** @brief Minimum readings required before performing aggregation */
  static constexpr uint8_t MIN_READINGS_FOR_AGGREGATION = 3;

  /**
   * @brief Default aggregation structure with NaN values
   * Used when no valid data is available for aggregation
   * Default values pulled from AanderaaConductivityAggregations definition
   */
  static constexpr AanderaaConductivityAggregations aanderaa_conductivity_NAN_AGG = {};

  /**
   * @brief Constructor - initializes all samplers and counters
   */
  AanderaaConductivitySensor(uint64_t node_id, uint32_t agg_period_ms,
                                                      uint32_t averager_max_samples);

  /**
   * @brief Subscribe to conductivity sensor data topic
   * @return true if subscription successful, false otherwise
   */
  bool subscribe(void) override;

  /**
    * @brief Aggregate collected sensor data and submit to report builder
    *
    * This function calculates statistical means for all sensor parameters
    * and submits the aggregated data to the bridge's reporting system.
    * Only aggregates if minimum number of readings have been collected.
    */
  void aggregate(void);

  /**
   * @brief Get sample member parameters for encoding sensor data
   * @param sensor_data Pointer to sensor data
   * @param sample_index Index of sample to encode
   * @param sampleMemberParams
   */
  static void get_sample_member_params(void *sensor_data, uint32_t sample_index, SampleMemberParams *params);

  /**
   * @brief Get report parameters for encoding sensor data
   * @param context Encoder context
   * @param sensor_data Pointer to sensor data
   * @param sample_index Index of sample to encode
   * @return Report parameters
   */
  static ReportParams get_report_params(sensor_report_encoder_context_t &context,
                                            void *sensor_data, uint32_t sample_index);

  /**
 * @brief Helper function to set up conductivity sensor data pointers.
 * @param element Pointer to the element in the linked list.
 * @param nan_sample Pointer to store the NAN sample reference.
 * @param dst Pointer to store the destination data reference.
 */
  static void setup_sensor_pointers(report_builder_element_t *element,
                                      const void **nan_sample,
                                      void **dst);

  /**
   * @brief Get the size of the aggregation data structure.
   * @return Size of the aggregation data structure.
   */
  static size_t get_aggregation_size(void) {
    return sizeof(AanderaaConductivityAggregations);
  };

  /**
   * @brief Get the default reading period for the sensor in milliseconds.
   * @return Default reading period in milliseconds.
   */
  static uint32_t get_default_reading_period_ms(void) {
    return DEFAULT_AANDERAA_CONDUCTIVITY_READING_PERIOD_MS;
  }

private:
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
  static void sub_callback(uint64_t node_id, const char *topic,
                                              uint16_t topic_len, const uint8_t *data,
                                              uint16_t data_len, uint8_t type, uint8_t version);

  /// Subtag for conductivity sensor data
  static constexpr char subtag[] = "/sofar/aanderaa_conductivity_data";

  /** @brief Default sensor reading period in milliseconds */
  static constexpr uint32_t DEFAULT_AANDERAA_CONDUCTIVITY_READING_PERIOD_MS = 30000; // default is 30 seconds

} AanderaaConductivity_t;

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
AanderaaConductivity_t *create(uint64_t node_id, uint32_t agg_period_ms,
                                                      uint32_t averager_max_samples);
