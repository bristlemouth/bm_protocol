#pragma once

#include "aanderaa_conductivity_msg.h"
#include "gtest/gtest.h"

/**
 * @file conductivity_test_helpers.h
 * @brief Shared test constants and helper functions for conductivity sensor testing
 *
 * This file provides standardized test data and verification functions that can be
 * reused across multiple test files (integration tests, unit tests, CBOR tests, etc.)
 * to ensure consistency and reduce duplication.
 */

namespace ConductivityTestData {
  // Header values - standardized across all conductivity tests
  constexpr uint64_t READING_TIME_UTC_MS = 1640995200000ULL;  // 2022-01-01 00:00:00 UTC
  constexpr uint32_t SENSOR_READING_TIME_MS = 1000;
  constexpr uint32_t READING_UPTIME_MILLIS = 5000;

  // Sensor values - realistic conductivity sensor readings
  constexpr double CONDUCTIVITY_MS_CM = 50.123;      // mS/cm - typical seawater
  constexpr double TEMPERATURE_DEG_C = 23.456;       // °C - typical ocean temp
  constexpr double SALINITY_PSU = 35.123;            // PSU - typical seawater salinity
  constexpr double WATER_DENSITY_KG_M3 = 1025.83;    // kg/m³ - typical seawater density
  constexpr double SOUND_SPEED_M_S = 1498.15;        // m/s - typical sound speed in seawater
  constexpr double DEPTH_M = 10.0;                   // m - shallow water depth

  // Tolerance values for floating point comparisons
  constexpr double PRECISION_TOLERANCE = 0.001;      // High precision for conductivity, temp, salinity
  constexpr double DENSITY_TOLERANCE = 0.01;         // Medium precision for density
  constexpr double SPEED_TOLERANCE = 0.01;           // Medium precision for sound speed
  constexpr double DEPTH_TOLERANCE = 0.01;           // Medium precision for depth
}

/**
 * @brief Create standard test data for conductivity sensor
 * @return AanderaaConductivityMsg::Data with standardized test values
 */
inline AanderaaConductivityMsg::Data createStandardTestData() {
  AanderaaConductivityMsg::Data data;
  data.header.reading_time_utc_ms = ConductivityTestData::READING_TIME_UTC_MS;
  data.header.sensor_reading_time_ms = ConductivityTestData::SENSOR_READING_TIME_MS;
  data.header.reading_uptime_millis = ConductivityTestData::READING_UPTIME_MILLIS;
  data.conductivity_ms_cm = ConductivityTestData::CONDUCTIVITY_MS_CM;
  data.temperature_deg_c = ConductivityTestData::TEMPERATURE_DEG_C;
  data.salinity_psu = ConductivityTestData::SALINITY_PSU;
  data.water_density_kg_m3 = ConductivityTestData::WATER_DENSITY_KG_M3;
  data.sound_speed_m_s = ConductivityTestData::SOUND_SPEED_M_S;
  data.depth_m = ConductivityTestData::DEPTH_M;
  return data;
}

/**
 * @brief Create test data with custom values (overrides defaults where specified)
 * @param conductivity_override Optional override for conductivity value
 * @param temperature_override Optional override for temperature value
 * @return AanderaaConductivityMsg::Data with custom values
 */
inline AanderaaConductivityMsg::Data createCustomTestData(
    double conductivity_override = ConductivityTestData::CONDUCTIVITY_MS_CM,
    double temperature_override = ConductivityTestData::TEMPERATURE_DEG_C) {
  auto data = createStandardTestData();
  data.conductivity_ms_cm = conductivity_override;
  data.temperature_deg_c = temperature_override;
  return data;
}

/**
 * @brief Verify header data integrity between expected and actual
 * @param expected Expected header data
 * @param actual Actual header data to verify
 */
inline void verifyHeaderIntegrity(const AanderaaConductivityMsg::Data& expected,
                                 const AanderaaConductivityMsg::Data& actual) {
  EXPECT_EQ(actual.header.reading_time_utc_ms, expected.header.reading_time_utc_ms);
  EXPECT_EQ(actual.header.sensor_reading_time_ms, expected.header.sensor_reading_time_ms);
  EXPECT_EQ(actual.header.reading_uptime_millis, expected.header.reading_uptime_millis);
}

/**
 * @brief Verify sensor data integrity between expected and actual
 * @param expected Expected sensor data
 * @param actual Actual sensor data to verify
 */
inline void verifySensorDataIntegrity(const AanderaaConductivityMsg::Data& expected,
                                     const AanderaaConductivityMsg::Data& actual) {
  EXPECT_NEAR(actual.conductivity_ms_cm, expected.conductivity_ms_cm, ConductivityTestData::PRECISION_TOLERANCE);
  EXPECT_NEAR(actual.temperature_deg_c, expected.temperature_deg_c, ConductivityTestData::PRECISION_TOLERANCE);
  EXPECT_NEAR(actual.salinity_psu, expected.salinity_psu, ConductivityTestData::PRECISION_TOLERANCE);
  EXPECT_NEAR(actual.water_density_kg_m3, expected.water_density_kg_m3, ConductivityTestData::DENSITY_TOLERANCE);
  EXPECT_NEAR(actual.sound_speed_m_s, expected.sound_speed_m_s, ConductivityTestData::SPEED_TOLERANCE);
  EXPECT_NEAR(actual.depth_m, expected.depth_m, ConductivityTestData::DEPTH_TOLERANCE);
}

/**
 * @brief Verify complete data integrity (header + sensor data)
 * @param expected Expected complete data
 * @param actual Actual complete data to verify
 */
inline void verifyCompleteDataIntegrity(const AanderaaConductivityMsg::Data& expected,
                                       const AanderaaConductivityMsg::Data& actual) {
  verifyHeaderIntegrity(expected, actual);
  verifySensorDataIntegrity(expected, actual);
}
