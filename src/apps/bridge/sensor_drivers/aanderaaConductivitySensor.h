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
#include "FreeRTOS.h"
#include "abstractSensor.h"
#include "avgSampler.h"
#include "sensorController.h"
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
  double conductivity_mean_ms_cm;      ///< Mean conductivity in mS/cm
  double temperature_mean_deg_c;       ///< Mean temperature in degrees Celsius
  double salinity_mean_psu;            ///< Mean salinity in Practical Salinity Units
  double water_density_mean_kg_m3;     ///< Mean water density in kg/m³
  double sound_speed_mean_m_s;         ///< Mean sound speed in m/s
  double depth_mean_m;                 ///< Mean depth in meters
  uint32_t reading_count;              ///< Number of readings used in aggregation
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
  uint32_t current_agg_period_ms;      ///< Current aggregation period in milliseconds

  // Statistical samplers for each sensor parameter
  AveragingSampler conductivity_ms_cm;     ///< Conductivity sampler (mS/cm)
  AveragingSampler temperature_deg_c;      ///< Temperature sampler (°C)
  AveragingSampler salinity_psu;           ///< Salinity sampler (PSU)
  AveragingSampler water_density_kg_m3;    ///< Water density sampler (kg/m³)
  AveragingSampler sound_speed_m_s;        ///< Sound speed sampler (m/s)
  AveragingSampler depth_m;                ///< Depth sampler (m)

  uint32_t reading_count;              ///< Total number of readings received
  int8_t node_position;                ///< Position of this node in sensor array
  uint32_t last_timestamp;             ///< Timestamp of last received reading

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
  AanderaaConductivitySensor()
      : conductivity_ms_cm(), temperature_deg_c(), salinity_psu(), water_density_kg_m3(),
        sound_speed_m_s(), depth_m(), reading_count(0), node_position(-1), last_timestamp(0) {}

public:
  /**
   * @brief Subscribe to conductivity sensor data topic
   * @return true if subscription successful, false otherwise
   */
  bool subscribe() override;

  /**
   * @brief Aggregate collected sensor data and add to report queue
   */
  void aggregate(void);

  /**
   * @brief Default aggregation structure with NaN values
   * Used when no valid data is available for aggregation
   */
  static constexpr aanderaa_conductivity_aggregations_t aanderaa_conductivity_NAN_AGG = {
      .conductivity_mean_ms_cm = NAN,
      .temperature_mean_deg_c = NAN,
      .salinity_mean_psu = NAN,
      .water_density_mean_kg_m3 = NAN,
      .sound_speed_mean_m_s = NAN,
      .depth_mean_m = NAN,
      .reading_count = 0,
  };

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
  /** @brief Topic subtag for conductivity sensor data */
  static constexpr char subtag[] = "/sofar/aanderaa_conductivity_data";
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
