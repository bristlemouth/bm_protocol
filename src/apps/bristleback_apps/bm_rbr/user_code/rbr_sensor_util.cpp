#include "rbr_sensor_util.h"
#include "FreeRTOS.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

namespace BmRbrSensorUtil {

static bool validSpecialChar(char c);

/*!
    * @brief Check if the sensor data string is valid
    * @param[in] s The sensor data string
    * @param[in] strlen The length of the sensor data string
    * @return True if the sensor data string is valid, false otherwise
    */
bool validSensorDataString(const char *s, size_t strlen) {
  configASSERT(s);
  bool rval = false;
  do {
    // 78 determined by 1.5 the maximum length of the longest sensor data string
    // coming from the RBRcoda3 T.ODO sensor
    // https://rbr-global.com/wp-content/uploads/2023/12/RBR-Sensors-RUG-0003296revN.pdf
    if (strlen == 0 || strlen > 78) {
      break;
    }
    bool invalid = false;
    for (size_t i = 0; i < strlen; i++) {
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
    * @note From https://rbr-global.com/products/sensors/rbrcoda-td/
*/
bool validSensorData(DataType_e type, double val) {
  switch (type) {
  case TEMPERATURE: // in degrees C
    return val >= -5.0 && val <= 35.0;
  case PRESSURE: // in decibar
    return val >= 5.0 && val <= 85.0;
  default:
    return false;
  }
}

/*!
    * @brief Check if the sensor output format configuration is valid
    * @param[in] s The sensor output format
    * @param[in] strlen The length of the sensor output format
    * @return True if the sensor output format is valid, false otherwise
    * @note See outputformat channelslist in https://rbr-global.com/wp-content/uploads/2023/12/RBR-Sensors-RUG-0003296revN.pdf
*/
bool validSensorOutputformat(const char *s, size_t strlen) {
  configASSERT(s);
  bool rval = false;
  do {
    if (strlen == 0) {
      break;
    }
    if (strstr(s, "outputformat channelslist =") == NULL) {
      break;
    }
    if (strstr(s, "pressure(dbar)") == NULL && strstr(s, "temperature(C)") == NULL) {
      break;
    }
    rval = true;
  } while (0);
  return rval;
}

static bool validSpecialChar(char c) {
  switch (c) {
  case ',':
  case '.':
    return true;
  default:
    return false;
  }
}

} // namespace BmRbrSensorUtil