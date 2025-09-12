#include "AanderaaConductivitySensorUtil.h"
#include "FreeRTOS.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

namespace AanderaaConductivitySensorUtil {

static bool validSpecialChar(char c);

/*!
 * @brief Check if the sensor data string is valid
 * @param[in] s The sensor data string
 * @param[in] len The length of the sensor data string
 * @return True if the sensor data string is valid, false otherwise
 */
bool validSensorDataString(const char *s, size_t len) {
  configASSERT(s);
  bool rval = false;
  do {
    // Maximum expected length for Aanderaa 5990 conductivity sensor CSV output
    // Format: conductivity,temperature,salinity,density,sound_speed,depth
    // Estimated max: 15.1234,25.5678,35.1234,1025.8765,1498.1234,10.5 = ~65 chars
    if (len == 0 || len > 100) {
      break;
    }
    bool invalid = false;
    for (size_t i = 0; i < len; i++) {
      if ((!isdigit(s[i]) && !isspace(s[i]) && !validSpecialChar(s[i]))) {
        invalid = true;
        break;
      }
    }
    if (invalid) {
      break;
    }
    rval = true;
  } while (0);
  return rval;
}

/*!
 * @brief Check if the sensor data is valid
 * @param[in] type The type of the sensor data
 * @param[in] val The value of the sensor data
 * @return True if the sensor data is valid, false otherwise
 * @note Based on Aanderaa 5990 Conductivity Sensor specifications
 */
bool validSensorData(DataType_e type, double val) {
  switch (type) {
  case CONDUCTIVITY: // in mS/cm
    return val >= 0.0 && val <= 100.0;
  case TEMPERATURE: // in degrees C
    return val >= -5.0 && val <= 45.0;
  case SALINITY: // in PSU
    return val >= 0.0 && val <= 50.0;
  case WATER_DENSITY: // in kg/m³
    return val >= 990.0 && val <= 1050.0;
  case SOUND_SPEED: // in m/s
    return val >= 1400.0 && val <= 1600.0;
  case DEPTH: // in meters
    return val >= 0.0 && val <= 1000.0;
  default:
    return false;
  }
}

void preprocessLine(char *str, uint16_t &len) {
  configASSERT(str);
  if (!len) {
    return;
  }
  // Remove any trailing whitespace/newlines
  while (len > 0 && (str[len-1] == '\n' || str[len-1] == '\r' || str[len-1] == ' ')) {
    str[len-1] = '\0';
    len--;
  }
}

static bool validSpecialChar(char c) {
  switch (c) {
  case ',':
  case '.':
  case '-':
    return true;
  default:
    return false;
  }
}

} // namespace AanderaaConductivitySensorUtil
