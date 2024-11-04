#include "fff.h"
#include "rbr_sensor_util.h"
#include "gtest/gtest.h"
#include <string.h>

DEFINE_FFF_GLOBALS;

using namespace testing;
using namespace BmRbrSensorUtil;

TEST(rbrSensorUtilTest, validString) {
  static constexpr char temp[] = "14500, 17.7684\n";
  static constexpr char pressure[] = "332500, 9.9915\n";
  static constexpr char pressureAndTemp[] = "332500, 9.9915, 17.7684\n";
  EXPECT_TRUE(validSensorDataString(temp, strlen(temp)));
  EXPECT_TRUE(validSensorDataString(pressure, strlen(pressure)));
  EXPECT_TRUE(validSensorDataString(pressureAndTemp, strlen(pressureAndTemp)));
}

TEST(rbrSensorUtilTest, InvalidString) {
  static constexpr char temp[] = "145@0, 17.7684\n";
  static constexpr char pressure[] = "33250!, 9.9915\n";
  static constexpr char pressureAndTemp[] = "332500, 9.\\, 17.7684\n";
  static constexpr char tooLongString[] =
      "332500, 9.9915, 17.7684,332500, 9.9915, 17.7684,332500, 9.9915, 17.7684,43240 \n";
  EXPECT_FALSE(validSensorDataString(temp, strlen(temp)));
  EXPECT_FALSE(validSensorDataString(pressure, strlen(pressure)));
  EXPECT_FALSE(validSensorDataString(pressureAndTemp, strlen(pressureAndTemp)));
  EXPECT_FALSE(validSensorDataString(tooLongString, strlen(tooLongString)));
}

TEST(rbrSensorUtilTest, validData) {
  EXPECT_TRUE(validSensorData(DataType_e::TEMPERATURE, 17.7684));
  EXPECT_TRUE(validSensorData(DataType_e::TEMPERATURE, 125.0));
  EXPECT_TRUE(validSensorData(DataType_e::TEMPERATURE, -40.0));
  EXPECT_TRUE(validSensorData(DataType_e::PRESSURE, 200.0));
  EXPECT_TRUE(validSensorData(DataType_e::PRESSURE, 5.0));
  EXPECT_TRUE(validSensorData(DataType_e::PRESSURE, 45));
}

TEST(rbrSensorUtilTest, InvalidData) {
  EXPECT_FALSE(validSensorData(DataType_e::TEMPERATURE, 125.1));
  EXPECT_FALSE(validSensorData(DataType_e::TEMPERATURE, -40.1));
  EXPECT_FALSE(validSensorData(DataType_e::PRESSURE, -10));
  EXPECT_FALSE(validSensorData(DataType_e::PRESSURE, 4.9));
  EXPECT_FALSE(validSensorData(DataType_e::PRESSURE, 200.1));
}

TEST(rbrSensorUtilTest, validOutputFormat) {
  static constexpr char temp[] = "outputformat channelslist = temperature(C)";
  static constexpr char pressure[] = "outputformat channelslist = pressure(dbar)";
  static constexpr char pressureAndTemp[] =
      "outputformat channelslist = temperature(C)|pressure(dbar)";
  EXPECT_TRUE(validSensorOutputformat(temp, strlen(temp)));
  EXPECT_TRUE(validSensorOutputformat(pressure, strlen(pressure)));
  EXPECT_TRUE(validSensorOutputformat(pressureAndTemp, strlen(pressureAndTemp)));
}

TEST(rbrSensorUtilTest, InvalidOutputFormat) {
  static constexpr char temp[] = "list = temperature5600, 17.7684";
  static constexpr char pressure[] = "st = pressure(dbar)0500, 10.1614";
  static constexpr char pressureAndTemp[] = "erature(C)|pressure(dbar)53000, 23.2246, 10.1527)";
  static constexpr char mangle1[] = "outputformat channelslist = temperature";
  static constexpr char mangle2[] = "outputformat channelslist = pressure";
  static constexpr char mangle3[] = "outputformat channelslist=";
  EXPECT_FALSE(validSensorOutputformat(temp, strlen(temp)));
  EXPECT_FALSE(validSensorOutputformat(pressure, strlen(pressure)));
  EXPECT_FALSE(validSensorOutputformat(pressureAndTemp, strlen(pressureAndTemp)));
  EXPECT_FALSE(validSensorOutputformat(mangle1, strlen(mangle1)));
  EXPECT_FALSE(validSensorOutputformat(mangle2, strlen(mangle2)));
  EXPECT_FALSE(validSensorOutputformat(mangle3, strlen(mangle3)));
}

TEST(rbrSensorUtilTest, parseoutReady) {
  static constexpr char parsedReady[] = "96500, 10.1841";
  char readystr[] = "Ready: 96500, 10.1841";
  uint16_t line_len = strlen(readystr);
  preprocessLine(readystr, line_len);
  EXPECT_EQ(line_len, strlen(parsedReady));
  EXPECT_STREQ(readystr, parsedReady);
}
