#include "rbr_sensor.h"
#include "FreeRTOS.h"
#include "OrderedSeparatorLineParser.h"
#include "bm_printf.h"
#include "configuration.h"
#include "payload_uart.h"
#include "rbr_sensor_util.h"
#include "serial.h"
#include "stm32_rtc.h"
#include "task_priorities.h"
#include "uptime.h"
#include <ctype.h>
#include <math.h>
#include <string.h>

extern cfg::Configuration *systemConfigurationPartition;

/*!
* @brief Initialize the RBR sensor driver.
* @param type The sensor type.
*/
void RbrSensor::init(BmRbrDataMsg::SensorType_t type) {
  _type = type;
  _stored_type = type;
  for (int i = 0; i < NUM_PARSERS; i++) {
    if (_parsers[i]) {
      _parsers[i]->init();
    }
  }
  PLUART::init(USER_TASK_PRIORITY);
  // Baud set per expected baud rate of the sensor.
  PLUART::setBaud(BAUD_RATE);
  // Disable passing raw bytes to user app.
  PLUART::setUseByteStreamBuffer(false);
  // Enable parsing lines and passing to user app.
  /// Warning: PLUART only stores a single line at a time. If your attached payload sends lines
  /// faster than the app reads them, they will be overwritten and data will be lost.
  PLUART::setUseLineBuffer(true);
  // Set a line termination character per protocol of the sensor.
  PLUART::setTerminationCharacter(LINE_TERM);
  // Turn on the UART.
  PLUART::enable();
}

/*!
* @brief Probe the sensor type.
* @param timeout_ms The timeout in milliseconds.
* @return True if the sensor type was successfully probed, false otherwise.
*/
bool RbrSensor::probeType(uint32_t timeout_ms) {
  uint32_t start = uptimeGetMs();
  PLUART::write((uint8_t *)typeCommand, strlen(typeCommand));
  bool success = false;
  do {
    if (PLUART::lineAvailable()) {
      PLUART::readLine(_payload_buffer, sizeof(_payload_buffer));
      if (BmRbrSensorUtil::validSensorOutputformat(_payload_buffer, strlen(_payload_buffer))) {
        if (strstr(_payload_buffer, "pressure") != NULL &&
            strstr(_payload_buffer, "temperature") != NULL) {
          if (_stored_type != BmRbrDataMsg::SensorType::PRESSURE_AND_TEMPERATURE) {
            systemConfigurationPartition->setConfig(
                "rbrCodaType", strlen("rbrCodaType"),
                static_cast<uint32_t>(BmRbrDataMsg::SensorType::PRESSURE_AND_TEMPERATURE));
            bm_fprintf(0, RBR_RAW_LOG, "Detected temp/pressure sensor, saving config\n");
            bm_printf(0, "Detected temp/pressure sensor, saving config\n");
            printf("Detected temp/pressure sensor, saving config.\n");
            systemConfigurationPartition->saveConfig(); // reboot
          }
          _type = BmRbrDataMsg::SensorType::PRESSURE_AND_TEMPERATURE;
          success = true;
          break;
        } else if (strstr(_payload_buffer, "pressure") != NULL) {
          if (_stored_type != BmRbrDataMsg::SensorType::PRESSURE) {
            systemConfigurationPartition->setConfig(
                "rbrCodaType", strlen("rbrCodaType"),
                static_cast<uint32_t>(BmRbrDataMsg::SensorType::PRESSURE));
            bm_fprintf(0, RBR_RAW_LOG, "Detected pressure sensor, saving config\n");
            bm_printf(0, "Detected pressure sensor, saving config\n");
            printf("Detected pressure sensor, saving config.\n");
            systemConfigurationPartition->saveConfig(); // reboot
          }
          _type = BmRbrDataMsg::SensorType::PRESSURE;
          success = true;
          break;
        } else if (strstr(_payload_buffer, "temperature") != NULL) {
          if (_stored_type != BmRbrDataMsg::SensorType::TEMPERATURE) {
            systemConfigurationPartition->setConfig(
                "rbrCodaType", strlen("rbrCodaType"),
                static_cast<uint32_t>(BmRbrDataMsg::SensorType::TEMPERATURE));
            bm_fprintf(0, RBR_RAW_LOG, "Detected temp sensor, saving config\n");
            bm_printf(0, "Detected temp sensor, saving config\n");
            printf("Detected temp sensor, saving config.\n");
            systemConfigurationPartition->saveConfig(); // reboot
          }
          _type = BmRbrDataMsg::SensorType::TEMPERATURE;
          success = true;
          break;
        } else {
          bm_fprintf(0, RBR_RAW_LOG, "Invalid outputformat: %s\n", _payload_buffer);
          bm_printf(0, "Invalid outputformat: %s\n", _payload_buffer);
          printf("Invalid outputformat: %s\n", _payload_buffer);
        }
      } else {
        bm_fprintf(0, RBR_RAW_LOG, "Invalid outputformat: %s\n", _payload_buffer);
        bm_printf(0, "Invalid outputformat: %s\n", _payload_buffer);
        printf("Invalid outputformat: %s\n", _payload_buffer);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  } while (uptimeGetMs() - start < timeout_ms);
  if (!success) {
    if (++_sensorDropDebounceCount == SENSOR_DROP_DEBOUNCE_MAX_COUNT) {
      _type = BmRbrDataMsg::SensorType::UNKNOWN;
      bm_fprintf(0, RBR_RAW_LOG, "RBR sensor was lost\n");
      bm_printf(0, "RBR sensor was lost\n");
      printf("RBR sensor was lost\n");
    }
  } else {
    if (_sensorDropDebounceCount >= SENSOR_DROP_DEBOUNCE_MAX_COUNT) {
      bm_fprintf(0, RBR_RAW_LOG, "RBR sensor online\n");
      bm_printf(0, "RBR sensor online\n");
      printf("RBR sensor online\n");
    }
    _sensorDropDebounceCount = 0;
  }
  return success;
}

/*!
  * @brief Get the sensor data from the payload.
  * @param d The sensor data to be filled in.
  * @return True if the sensor data was successfully read and parsed.
*/
bool RbrSensor::getData(BmRbrDataMsg::Data &d) {
  bool rval = false;
  do {
    // Read a line if it is available
    if (PLUART::lineAvailable()) {
      uint16_t read_len = PLUART::readLine(_payload_buffer, sizeof(_payload_buffer));

      // Get the RTC if available
      RTCTimeAndDate_t time_and_date = {};
      rtcGet(&time_and_date);
      char rtcTimeBuffer[32] = {};
      rtcPrint(rtcTimeBuffer, NULL);
      bm_fprintf(0, RBR_RAW_LOG, "tick: %" PRIu64 ", rtc: %s, line: %.*s\n", uptimeGetMs(),
                 rtcTimeBuffer, read_len, _payload_buffer);
      bm_printf(0, "rbr | tick: %" PRIu64 ", rtc: %s, line: %.*s", uptimeGetMs(), rtcTimeBuffer,
                read_len, _payload_buffer);
      printf("rbr | tick: %" PRIu64 ", rtc: %s, line: %.*s\n", uptimeGetMs(), rtcTimeBuffer,
             read_len, _payload_buffer);

      BmRbrSensorUtil::preprocessLine(_payload_buffer, read_len);

      if (!BmRbrSensorUtil::validSensorDataString(_payload_buffer, read_len)) {
        bm_fprintf(0, RBR_RAW_LOG, "Invalid sensor data string: %.*s\n", read_len,
                   _payload_buffer);
        bm_printf(0, "Invalid sensor data string: %.*s\n", read_len, _payload_buffer);
        printf("Invalid sensor data string: %.*s\n", read_len, _payload_buffer);
        break;
      }

      // Now when we get a line of text data, our LineParser turns it into numeric values.
      if (_parsers[_type] && _parsers[_type]->parseLine(_payload_buffer, read_len)) {
        switch (_type) {
        case BmRbrDataMsg::SensorType::TEMPERATURE: {
          Value timeValue = _parsers[_type]->getValue(0);
          Value tempValue = _parsers[_type]->getValue(1);
          if (timeValue.type != TYPE_UINT64 || tempValue.type != TYPE_DOUBLE) {
            printf("Parsed invalid values: time: %d, temp: %d\n", timeValue.type,
                   tempValue.type);
            break;
          }
          if (!BmRbrSensorUtil::validSensorData(BmRbrSensorUtil::TEMPERATURE,
                                                tempValue.data.double_val)) {
            printf("Invalid temperature value: %lf\n", tempValue.data.double_val);
            break;
          }
          d.sensor_type = BmRbrDataMsg::SensorType::TEMPERATURE;
          d.header.reading_time_utc_ms = rtcGetMicroSeconds(&time_and_date) / 1000;
          d.header.reading_uptime_millis = uptimeGetMs();
          d.header.sensor_reading_time_ms = timeValue.data.uint64_val;
          d.temperature_deg_c = tempValue.data.double_val;
          d.pressure_deci_bar = NAN;
          rval = true;
          break;
        }
        case BmRbrDataMsg::SensorType::PRESSURE: {
          Value timeValue = _parsers[_type]->getValue(0);
          Value pressureValue = _parsers[_type]->getValue(1);
          if (timeValue.type != TYPE_UINT64 || pressureValue.type != TYPE_DOUBLE) {
            printf("Parsed invalid types: time: %d, pressure: %d\n", timeValue.type,
                   pressureValue.type);
            break;
          }
          if (!BmRbrSensorUtil::validSensorData(BmRbrSensorUtil::PRESSURE,
                                                pressureValue.data.double_val)) {
            printf("Invalid pressure value: %lf\n", pressureValue.data.double_val);
            break;
          }
          d.sensor_type = BmRbrDataMsg::SensorType::PRESSURE;
          d.header.reading_time_utc_ms = rtcGetMicroSeconds(&time_and_date) / 1000;
          d.header.reading_uptime_millis = uptimeGetMs();
          d.header.sensor_reading_time_ms = timeValue.data.uint64_val;
          d.temperature_deg_c = NAN;
          d.pressure_deci_bar = pressureValue.data.double_val;
          rval = true;
          break;
        }
        case BmRbrDataMsg::SensorType::PRESSURE_AND_TEMPERATURE: {
          Value timeValue = _parsers[_type]->getValue(0);
          Value tempValue = _parsers[_type]->getValue(1);
          Value pressureValue = _parsers[_type]->getValue(2);
          if (timeValue.type != TYPE_UINT64 || tempValue.type != TYPE_DOUBLE ||
              pressureValue.type != TYPE_DOUBLE) {
            printf("Parsed invalid types: time: %d, temp: %d, pressure: %d\n", timeValue.type,
                   tempValue.type, pressureValue.type);
            break;
          }
          if (!BmRbrSensorUtil::validSensorData(BmRbrSensorUtil::TEMPERATURE,
                                                tempValue.data.double_val)) {
            printf("Invalid temperature value: %lf\n", tempValue.data.double_val);
            break;
          }
          if (!BmRbrSensorUtil::validSensorData(BmRbrSensorUtil::PRESSURE,
                                                pressureValue.data.double_val)) {
            printf("Invalid pressure value: %lf\n", pressureValue.data.double_val);
            break;
          }
          d.sensor_type = BmRbrDataMsg::SensorType::PRESSURE_AND_TEMPERATURE;
          d.header.reading_time_utc_ms = rtcGetMicroSeconds(&time_and_date) / 1000;
          d.header.reading_uptime_millis = uptimeGetMs();
          d.header.sensor_reading_time_ms = timeValue.data.uint64_val;
          d.temperature_deg_c = tempValue.data.double_val;
          d.pressure_deci_bar = pressureValue.data.double_val;
          rval = true;
          break;
        }
        default: {
          printf("Invalid rbr sensor type: %d\n", _type);
          break;
        }
        }
      } else {
        printf("Error parsing line!\n");
        break;
      }
    }
  } while (0);
  return rval;
}

/*!
* @brief Flush the data from the sensor driver.
*/
void RbrSensor::flush(void) { PLUART::reset(); }
