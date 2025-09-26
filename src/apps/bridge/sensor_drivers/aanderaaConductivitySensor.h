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
#include "avgSampler.h"
#include "sensorController.h"
#include "reportBuilder.h"
#include "reportBuilderList.h"
#include <cmath>
#include <stdint.h>
#include <stdlib.h>

/** @brief Number of sensor parameters measured by the Aanderaa conductivity sensor */
#define AANDERAA_CONDUCTIVITY_NUM_SAMPLE_MEMBERS 6

/**
 * @brief Aggregated conductivity sensor data structure
 *
 * Contains the averaged/aggregated values from multiple conductivity sensor readings.
 * This structure is used for storing and transmitting aggregated sensor data
 * in the bridge's report system.
 */
typedef struct aanderaa_conductivity_aggregations_s {
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
} aanderaa_conductivity_aggregations_t;

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
  /// Aggregation period in milliseconds
  uint32_t current_agg_period_ms;

  // Statistical samplers for each sensor parameter
  /// Conductivity sampler (mS/cm)
  AveragingSampler conductivity_ms_cm;
  /// Temperature sampler (°C)
  AveragingSampler temperature_deg_c;
  /// Salinity sampler (PSU)
  AveragingSampler salinity_psu;
  /// Water density sampler (kg/m³)
  AveragingSampler water_density_kg_m3;
  /// Sound speed sampler (m/s)
  AveragingSampler sound_speed_m_s;
  /// Depth sampler (m)
  AveragingSampler depth_m;

  /// Total number of readings received
  uint32_t reading_count;
  /// Position of this node in sensor array
  int8_t node_position;
  /// Timestamp of last received reading
  uint32_t last_timestamp;

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
   * @brief Constructor - initializes all samplers and counters
   */
  AanderaaConductivitySensor(void)
      : conductivity_ms_cm(), temperature_deg_c(), salinity_psu(), water_density_kg_m3(),
        sound_speed_m_s(), depth_m(), reading_count(0),
        node_position(-1), last_timestamp(0) {}

public:
  /**
   * @brief Subscribe to conductivity sensor data topic
   * @return true if subscription successful, false otherwise
   */
  bool subscribe(void) override;

  /**
   * @brief Aggregate collected sensor data and add to report queue
   */
  void aggregate(void);

  /**
   * @brief Get sample member parameters for encoding sensor data
   * @param sensor_data Pointer to sensor data
   * @param sample_index Index of sample to encode
   * @param sampleMemberParams
   */
  static void getSampleMemberParams(void *sensor_data, uint32_t sample_index, SampleMemberParams *params);

  /**
   * @brief Get report parameters for encoding sensor data
   * @param context Encoder context
   * @param sensor_data Pointer to sensor data
   * @param sample_index Index of sample to encode
   * @return Report parameters
   */
  static ReportParams getReportParams(sensor_report_encoder_context_t &context,
                                            void *sensor_data, uint32_t sample_index);

  /**
 * @brief Helper function to set up conductivity sensor data pointers.
 * @param element Pointer to the element in the linked list.
 * @param nan_sample Pointer to store the NAN sample reference.
 * @param dst Pointer to store the destination data reference.
 */
  static void setupSensorPointers(report_builder_element_t *element,
                                      const void **nan_sample,
                                      void **dst);

  /**
   * @brief Get the size of the aggregation data structure.
   * @return Size of the aggregation data structure.
   */
  static size_t getAggregationSize(void) {
    return sizeof(aanderaa_conductivity_aggregations_t);
  };


  /**
   * @brief Default aggregation structure with NaN values
   * Used when no valid data is available for aggregation
   * Default values pulled from aanderaa_conductivity_aggregations_t definition
   */
  static constexpr aanderaa_conductivity_aggregations_t aanderaa_conductivity_NAN_AGG = {};

  /**
   * @brief Get the default reading period for the sensor in milliseconds.
   * @return Default reading period in milliseconds.
   */
  static uint32_t getDefaultReadingPeriodMs(void) {
    return DEFAULT_AANDERAA_CONDUCTIVITY_READING_PERIOD_MS;
  }

private:
  /**
   * @brief Static callback for handling incoming conductivity sensor data
   * @param node_id Source node ID
   * @param topic MQTT topic string
   * @param topic_len Length of topic string
   * @param data CBOR-encoded sensor data
   * @param data_len Length of data buffer
   * @param type Message type (unused)
   * @param version Message version (unused)
   */
  static void aanderaaConductivitySubCallback(uint64_t node_id, const char *topic,
                                              uint16_t topic_len, const uint8_t *data,
                                              uint16_t data_len, uint8_t type, uint8_t version);

private:
  /// Subtag for conductivity sensor data
  static constexpr char subtag[] = "/sofar/aanderaa_conductivity_data";

  /** @brief Default sensor reading period in milliseconds */
  static constexpr uint32_t DEFAULT_AANDERAA_CONDUCTIVITY_READING_PERIOD_MS = 30000; // default is 30 seconds

} AanderaaConductivity_t;

/**
 * @brief Factory function to create and configure a conductivity sensor instance
 * @param node_id Target node ID to monitor
 * @param agg_period_ms Aggregation period in milliseconds
 * @param averager_max_samples Maximum samples for averaging
 * @return Pointer to configured sensor instance, or nullptr on failure
 */
AanderaaConductivity_t *createAanderaaConductivitySub(uint64_t node_id, uint32_t agg_period_ms,
                                                      uint32_t averager_max_samples);
