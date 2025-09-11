#include "fff.h"
#include "aanderaa_conductivity_sensor_util.h"
#include "gtest/gtest.h"
#include <string.h>

DEFINE_FFF_GLOBALS;

using namespace testing;
using namespace AanderaaConductivitySensorUtil;

TEST(aanderaaConductivitySensorUtilTest, validString) {
  static constexpr char conductivity_only[] = "50.123\n";
  static constexpr char full_data[] = "50.123,23.456,35.123,1025.83,1498.15,10.0\n";
  static constexpr char with_negatives[] = "45.678,-2.345,30.567,1020.45,1485.67,5.5\n";
  static constexpr char minimal[] = "0.0,0.0,0.0,990.0,1400.0,0.0\n";

  EXPECT_TRUE(validSensorDataString(conductivity_only, strlen(conductivity_only)));
  EXPECT_TRUE(validSensorDataString(full_data, strlen(full_data)));
  EXPECT_TRUE(validSensorDataString(with_negatives, strlen(with_negatives)));
  EXPECT_TRUE(validSensorDataString(minimal, strlen(minimal)));
}

TEST(aanderaaConductivitySensorUtilTest, InvalidString) {
  static constexpr char invalid_chars[] = "50.1@3,23.456,35.123,1025.83,1498.15,10.0\n";
  static constexpr char too_long_string[] =
      "50.123456789012345678901234567890,23.456789012345678901234567890,35.123456789012345678901234567890,1025.83456789012345678901234567890,1498.15456789012345678901234567890,10.0456789012345678901234567890\n";
  static constexpr char special_chars[] = "50.1#3,23.4$6,35.1%3,1025.8&,1498.1*,10.0!\n";
  static constexpr char empty_string[] = "";

  EXPECT_FALSE(validSensorDataString(invalid_chars, strlen(invalid_chars)));
  EXPECT_FALSE(validSensorDataString(too_long_string, strlen(too_long_string)));
  EXPECT_FALSE(validSensorDataString(special_chars, strlen(special_chars)));
  EXPECT_FALSE(validSensorDataString(empty_string, strlen(empty_string)));
}

TEST(aanderaaConductivitySensorUtilTest, validConductivityData) {
  EXPECT_TRUE(validSensorData(DataType_e::CONDUCTIVITY, 0.0));
  EXPECT_TRUE(validSensorData(DataType_e::CONDUCTIVITY, 50.123));
  EXPECT_TRUE(validSensorData(DataType_e::CONDUCTIVITY, 100.0));
  EXPECT_TRUE(validSensorData(DataType_e::CONDUCTIVITY, 35.567));
}

TEST(aanderaaConductivitySensorUtilTest, invalidConductivityData) {
  EXPECT_FALSE(validSensorData(DataType_e::CONDUCTIVITY, -0.1));
  EXPECT_FALSE(validSensorData(DataType_e::CONDUCTIVITY, 100.1));
  EXPECT_FALSE(validSensorData(DataType_e::CONDUCTIVITY, -10.0));
  EXPECT_FALSE(validSensorData(DataType_e::CONDUCTIVITY, 150.0));
}

TEST(aanderaaConductivitySensorUtilTest, validTemperatureData) {
  EXPECT_TRUE(validSensorData(DataType_e::TEMPERATURE, -5.0));
  EXPECT_TRUE(validSensorData(DataType_e::TEMPERATURE, 23.456));
  EXPECT_TRUE(validSensorData(DataType_e::TEMPERATURE, 45.0));
  EXPECT_TRUE(validSensorData(DataType_e::TEMPERATURE, 0.0));
}

TEST(aanderaaConductivitySensorUtilTest, invalidTemperatureData) {
  EXPECT_FALSE(validSensorData(DataType_e::TEMPERATURE, -5.1));
  EXPECT_FALSE(validSensorData(DataType_e::TEMPERATURE, 45.1));
  EXPECT_FALSE(validSensorData(DataType_e::TEMPERATURE, -20.0));
  EXPECT_FALSE(validSensorData(DataType_e::TEMPERATURE, 60.0));
}

TEST(aanderaaConductivitySensorUtilTest, validSalinityData) {
  EXPECT_TRUE(validSensorData(DataType_e::SALINITY, 0.0));
  EXPECT_TRUE(validSensorData(DataType_e::SALINITY, 35.123));
  EXPECT_TRUE(validSensorData(DataType_e::SALINITY, 50.0));
  EXPECT_TRUE(validSensorData(DataType_e::SALINITY, 25.5));
}

TEST(aanderaaConductivitySensorUtilTest, invalidSalinityData) {
  EXPECT_FALSE(validSensorData(DataType_e::SALINITY, -0.1));
  EXPECT_FALSE(validSensorData(DataType_e::SALINITY, 50.1));
  EXPECT_FALSE(validSensorData(DataType_e::SALINITY, -10.0));
  EXPECT_FALSE(validSensorData(DataType_e::SALINITY, 100.0));
}

TEST(aanderaaConductivitySensorUtilTest, validWaterDensityData) {
  EXPECT_TRUE(validSensorData(DataType_e::WATER_DENSITY, 990.0));
  EXPECT_TRUE(validSensorData(DataType_e::WATER_DENSITY, 1025.83));
  EXPECT_TRUE(validSensorData(DataType_e::WATER_DENSITY, 1050.0));
  EXPECT_TRUE(validSensorData(DataType_e::WATER_DENSITY, 1000.5));
}

TEST(aanderaaConductivitySensorUtilTest, invalidWaterDensityData) {
  EXPECT_FALSE(validSensorData(DataType_e::WATER_DENSITY, 989.9));
  EXPECT_FALSE(validSensorData(DataType_e::WATER_DENSITY, 1050.1));
  EXPECT_FALSE(validSensorData(DataType_e::WATER_DENSITY, 500.0));
  EXPECT_FALSE(validSensorData(DataType_e::WATER_DENSITY, 1500.0));
}

TEST(aanderaaConductivitySensorUtilTest, validSoundSpeedData) {
  EXPECT_TRUE(validSensorData(DataType_e::SOUND_SPEED, 1400.0));
  EXPECT_TRUE(validSensorData(DataType_e::SOUND_SPEED, 1498.15));
  EXPECT_TRUE(validSensorData(DataType_e::SOUND_SPEED, 1600.0));
  EXPECT_TRUE(validSensorData(DataType_e::SOUND_SPEED, 1500.5));
}

TEST(aanderaaConductivitySensorUtilTest, invalidSoundSpeedData) {
  EXPECT_FALSE(validSensorData(DataType_e::SOUND_SPEED, 1399.9));
  EXPECT_FALSE(validSensorData(DataType_e::SOUND_SPEED, 1600.1));
  EXPECT_FALSE(validSensorData(DataType_e::SOUND_SPEED, 1000.0));
  EXPECT_FALSE(validSensorData(DataType_e::SOUND_SPEED, 2000.0));
}

TEST(aanderaaConductivitySensorUtilTest, validDepthData) {
  EXPECT_TRUE(validSensorData(DataType_e::DEPTH, 0.0));
  EXPECT_TRUE(validSensorData(DataType_e::DEPTH, 10.0));
  EXPECT_TRUE(validSensorData(DataType_e::DEPTH, 1000.0));
  EXPECT_TRUE(validSensorData(DataType_e::DEPTH, 500.5));
}

TEST(aanderaaConductivitySensorUtilTest, invalidDepthData) {
  EXPECT_FALSE(validSensorData(DataType_e::DEPTH, -0.1));
  EXPECT_FALSE(validSensorData(DataType_e::DEPTH, 1000.1));
  EXPECT_FALSE(validSensorData(DataType_e::DEPTH, -10.0));
  EXPECT_FALSE(validSensorData(DataType_e::DEPTH, 2000.0));
}

TEST(aanderaaConductivitySensorUtilTest, preprocessLineRemovesTrailingWhitespace) {
  char test_str[] = "50.123,23.456,35.123,1025.83,1498.15,10.0\n\r  ";
  uint16_t len = strlen(test_str);
  preprocessLine(test_str, len);

  EXPECT_STREQ(test_str, "50.123,23.456,35.123,1025.83,1498.15,10.0");
  EXPECT_EQ(len, strlen("50.123,23.456,35.123,1025.83,1498.15,10.0"));
}

TEST(aanderaaConductivitySensorUtilTest, preprocessLineHandlesEmptyString) {
  char test_str[] = "";
  uint16_t len = 0;
  preprocessLine(test_str, len);

  EXPECT_EQ(len, 0);
}
