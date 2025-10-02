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
 * - Salinity (PSU and Std. Deviation)
 * - Water density (kg/m³)
 * - Sound speed (m/s)
 * - Depth (m)
 *
 * Topic: sensor/{node_id}/sofar/aanderaa_conductivity_data
 * Message Format: AanderaaConductivityMsg (CBOR)
 * Sensor Type: SENSOR_TYPE_AANDERAA_CONDUCTIVITY (8)
 */

#pragma once
#include "aanderaa_conductivity_msg.h"
#include "abstractSensor.h"
#include "avgSampler.h"
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
typedef struct AanderaaConductivityAggregations {
  double conductivity_mean_ms_cm = NAN;
  double temperature_mean_deg_c = NAN;
  double salinity_mean_psu = NAN;
  double salinity_std_dev = NAN;
  double water_density_mean_kg_m3 = NAN;
  double sound_speed_mean_m_s = NAN;
  double depth_mean_m = NAN;
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
  uint32_t current_agg_period_ms;

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

    AveragingSampler &operator[](SamplerType type) { return data[static_cast<size_t>(type)]; }
    const AveragingSampler &operator[](SamplerType type) const {
      return data[static_cast<size_t>(type)];
    }
  };

  SamplerSet samplers;

  // Convenience aliases for pointer-to-member types into the CBOR message:
  // DPtr points to a double field, FPtr points to a float field.
  using DPtr = double AanderaaConductivityMsg::Data::*;
  using FPtr = float AanderaaConductivityMsg::Data::*; // for depth_m

  // Mapping table from a SamplerType to the corresponding:
  //   a) "mean" field inside AanderaaConductivityAggregations.
  //      Used when copying sampler statistics back into the
  //      aggregation struct.
  //   b) corresponding field in the AanderaaConductivityMsg::Data struct
  //      decoded from CBOR payload. Because some fields are double and one
  //      (depth) is float, we store them in a std::variant<DPtr,FPtr>.
  //      Later, std::visit is used to extract the field, promoting float
  //      to double for consistent sampler input.
  struct SamplerMap {
    SamplerType type;
    double AanderaaConductivityAggregations::*aggr_ptr;
    std::variant<DPtr, FPtr> cbor_ptr;
  };

  static constexpr SamplerMap kSamplerMap[] = {
      {
          SamplerType::Conductivity_ms_cm,
          &AanderaaConductivityAggregations::conductivity_mean_ms_cm,
          &AanderaaConductivityMsg::Data::conductivity_ms_cm,
      },
      {
          SamplerType::Temperature_deg_c,
          &AanderaaConductivityAggregations::temperature_mean_deg_c,
          &AanderaaConductivityMsg::Data::temperature_deg_c,
      },
      {
          SamplerType::Salinity_psu,
          &AanderaaConductivityAggregations::salinity_mean_psu,
          &AanderaaConductivityMsg::Data::salinity_psu,
      },
      {
          SamplerType::Water_density_kg_m3,
          &AanderaaConductivityAggregations::water_density_mean_kg_m3,
          &AanderaaConductivityMsg::Data::water_density_kg_m3,
      },
      {
          SamplerType::Sound_speed_m_s,
          &AanderaaConductivityAggregations::sound_speed_mean_m_s,
          &AanderaaConductivityMsg::Data::sound_speed_m_s,
      },
      {
          SamplerType::Depth_m,
          &AanderaaConductivityAggregations::depth_mean_m,
          &AanderaaConductivityMsg::Data::depth_m,
      },
  };

  uint32_t reading_count;
  int8_t node_position;
  uint32_t last_timestamp;

  static constexpr uint32_t AANDERAA_CONDUCTIVITY_NUM_SAMPLE_MEMBERS = 7;
  static constexpr uint8_t MIN_READINGS_FOR_AGGREGATION = 3;
  static constexpr char subtag[] = "/sofar/aanderaa_conductivity_data";
  static constexpr uint32_t DEFAULT_AANDERAA_CONDUCTIVITY_READING_PERIOD_MS = 30000;

  BmErr send_spotter_log_individual(const AanderaaConductivityMsg::Data &m);
  BmErr send_spotter_log_aggregate(const AanderaaConductivityAggregations &agg);
  static void sub_callback(uint64_t node_id, const char *topic, uint16_t topic_len,
                           const uint8_t *data, uint16_t data_len, uint8_t type,
                           uint8_t version);

public:
  /**
   * @brief Sample buffer padding for timing variations
   *
   * Calculated as: sample_frequency + bridge_on_period + safety_margin
   * Default: 1Hz * 120s + 30s = 150 samples
   * Accounts for timing slop between sensor sampling and bridge aggregation periods.
   */
  static constexpr uint32_t N_SAMPLES_PAD = 150;

  /**
   * @brief Default aggregation structure with NaN values
   * Used when no valid data is available for aggregation
   * Default values pulled from AanderaaConductivityAggregations definition
   */
  static constexpr AanderaaConductivityAggregations aanderaa_conductivity_NAN_AGG = {};

  AanderaaConductivitySensor(uint64_t conductivity_node_id, uint32_t agg_period_ms,
                             uint32_t averager_max_samples);
  bool subscribe(void) override;
  void aggregate(void);
  static void get_report_builder_sample_params(void *sensor_data, uint32_t sample_index,
                                               ReportParams &report_params);
  static ReportParams get_report_params(sensor_report_encoder_context_t &context,
                                        void *sensor_data, uint32_t sample_index);
  static void setup_sensor_pointers(report_builder_element_t *element, const void **nan_sample,
                                    void **dst);

  /**
   * @brief Get the size of the aggregation data structure.
   * @return Size of the aggregation data structure.
   */
  static size_t get_aggregation_size(void) { return sizeof(AanderaaConductivityAggregations); };

  /**
   * @brief Get the default reading period for the sensor in milliseconds.
   * @return Default reading period in milliseconds.
   */
  static uint32_t get_default_reading_period_ms(void) {
    return DEFAULT_AANDERAA_CONDUCTIVITY_READING_PERIOD_MS;
  }

} AanderaaConductivity_t;

AanderaaConductivity_t *createAanderaaConductivitySub(uint64_t node_id, uint32_t agg_period_ms,
                                                      uint32_t averager_max_samples);
