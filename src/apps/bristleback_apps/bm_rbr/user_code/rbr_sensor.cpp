#include "rbr_sensor.h"
#include "FreeRTOS.h"
#include "OrderedSeparatorLineParser.h"
#include "spotter.h"
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

// Preventing typos. These strings are used several times.
static constexpr char PRESSURE[] = "pressure";
static constexpr char TEMPERATURE[] = "temperature";

/*!
* @brief Initialize the RBR sensor driver.
* @param type The sensor type.
* @param min_probe_period_ms How often to check the sensor type in milliseconds.
*/
void RbrSensor::init(BmRbrDataMsg::SensorType_t type, uint32_t min_probe_period_ms) {
  _type = type;
  _stored_type = type;
  _minProbePeriodMs = min_probe_period_ms;
  for (int i = 0; i < NUM_PARSERS; i++) {
    if (_parsers[i]) {
      _parsers[i]->init();
    }
  }

  get_config_uint(BM_CFG_PARTITION_SYSTEM, sensor_bm_log_enable, strlen(sensor_bm_log_enable),
                  &_sensorBmLogEnable);
  printf("sensorBmLogEnable: %" PRIu32 "\n", _sensorBmLogEnable);

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
* @brief Probe the sensor type if enough time has passed.
*/
void RbrSensor::maybeProbeType(uint64_t last_power_on_time) {
  uint64_t now = uptimeGetMs();
  if (now - _lastProbeTime >= _minProbePeriodMs &&
      now - last_power_on_time >= _minProbePeriodMs) {
    PLUART::write((uint8_t *)typeCommand, strlen(typeCommand));
    _lastProbeTime = uptimeGetMs();
    _awaitingProbeResponse = true;
  }
}

/*!
 * @brief Handle a line of output from the sensor payload.
 * @param d The sensor data to be filled in.
 * @return True if sensor data was successfully read and parsed.
 */
bool RbrSensor::getData(BmRbrDataMsg::Data &d) {
  bool success = false;
  if (PLUART::lineAvailable()) {
    uint16_t read_len = PLUART::readLine(_payload_buffer, sizeof(_payload_buffer));
    BmRbrSensorUtil::preprocessLine(_payload_buffer, read_len);
    if (BmRbrSensorUtil::validSensorOutputformat(_payload_buffer, read_len)) {
      handleOutputformat(_payload_buffer, read_len);
    } else if (BmRbrSensorUtil::validSensorDataString(_payload_buffer, read_len)) {
      success = handleDataString(_payload_buffer, read_len, d);
    } else {
      spotter_log(0, RBR_RAW_LOG, USE_TIMESTAMP, "Invalid line from sensor: %.*s\n", read_len,
                 _payload_buffer);
      spotter_log_console(0, "Invalid line from sensor: %.*s", read_len, _payload_buffer);
      printf("Invalid line from sensor: %.*s\n", read_len, _payload_buffer);
    }
  }

  // If we are awaiting a probe response, check for a timeout.
  if (_awaitingProbeResponse && uptimeGetMs() - _lastProbeTime >= PROBE_TYPE_TIMEOUT_MS) {
    _awaitingProbeResponse = false;
    _sensorDropDebounceCount++;
    if (_sensorDropDebounceCount == SENSOR_DROP_DEBOUNCE_MAX_COUNT) {
      _type = BmRbrDataMsg::SensorType::UNKNOWN;
      spotter_log(0, RBR_RAW_LOG, USE_TIMESTAMP, "RBR sensor was lost\n");
      spotter_log_console(0, "RBR sensor was lost");
      printf("RBR sensor was lost\n");
    }
  }

  return success;
}

void RbrSensor::handleOutputformat(const char *s, size_t read_len) {
  // If the sensor was previously lost, print a message that it is back online.
  if (_sensorDropDebounceCount >= SENSOR_DROP_DEBOUNCE_MAX_COUNT) {
    spotter_log(0, RBR_RAW_LOG, USE_TIMESTAMP, "RBR sensor online\n");
    spotter_log_console(0, "RBR sensor online");
    printf("RBR sensor online\n");
  }
  _sensorDropDebounceCount = 0;
  _awaitingProbeResponse = false;

  // Check whether the sensor type has changed.
  // If so, save the new type and reboot.
  if (strnstr(s, PRESSURE, read_len) != NULL && strnstr(s, TEMPERATURE, read_len) != NULL) {
    if (_stored_type != BmRbrDataMsg::SensorType::PRESSURE_AND_TEMPERATURE) {
      set_config_uint(
          BM_CFG_PARTITION_SYSTEM, CFG_RBR_TYPE, strlen(CFG_RBR_TYPE),
          static_cast<uint32_t>(BmRbrDataMsg::SensorType::PRESSURE_AND_TEMPERATURE));
      spotter_log(0, RBR_RAW_LOG, USE_TIMESTAMP,
                 "Detected temp & pressure sensor, saving config\n");
      spotter_log_console(0, "Detected temp & pressure sensor, saving config");
      printf("Detected temp & pressure sensor, saving config.\n");
      save_config(BM_CFG_PARTITION_SYSTEM, true); // reboot
    }
    _type = BmRbrDataMsg::SensorType::PRESSURE_AND_TEMPERATURE;
  } else if (strnstr(s, PRESSURE, read_len) != NULL) {
    if (_stored_type != BmRbrDataMsg::SensorType::PRESSURE) {
      set_config_uint(BM_CFG_PARTITION_SYSTEM, CFG_RBR_TYPE, strlen(CFG_RBR_TYPE),
                      static_cast<uint32_t>(BmRbrDataMsg::SensorType::PRESSURE));
      spotter_log(0, RBR_RAW_LOG, USE_TIMESTAMP, "Detected pressure sensor, saving config\n");
      spotter_log_console(0, "Detected pressure sensor, saving config");
      printf("Detected pressure sensor, saving config.\n");
      save_config(BM_CFG_PARTITION_SYSTEM, true); // reboot
    }
    _type = BmRbrDataMsg::SensorType::PRESSURE;
  } else if (strnstr(s, TEMPERATURE, read_len) != NULL) {
    if (_stored_type != BmRbrDataMsg::SensorType::TEMPERATURE) {
      set_config_uint(BM_CFG_PARTITION_SYSTEM, CFG_RBR_TYPE, strlen(CFG_RBR_TYPE),
                      static_cast<uint32_t>(BmRbrDataMsg::SensorType::TEMPERATURE));
      spotter_log(0, RBR_RAW_LOG, USE_TIMESTAMP, "Detected temp sensor, saving config\n");
      spotter_log_console(0, "Detected temp sensor, saving config");
      printf("Detected temp sensor, saving config.\n");
      save_config(BM_CFG_PARTITION_SYSTEM, true); // reboot
    }
    _type = BmRbrDataMsg::SensorType::TEMPERATURE;
  } else {
    spotter_log(0, RBR_RAW_LOG, USE_TIMESTAMP, "Invalid outputformat: %s\n", s);
    spotter_log_console(0, "Invalid outputformat: %s", s);
    printf("Invalid outputformat: %s\n", s);
  }
}

bool RbrSensor::handleDataString(const char *s, size_t read_len, BmRbrDataMsg::Data &d) {
  bool success = false;
  RTCTimeAndDate_t time_and_date = {};
  rtcGet(&time_and_date);
  char rtcTimeBuffer[32] = {};
  rtcPrint(rtcTimeBuffer, NULL);
  if (_sensorBmLogEnable) {
    spotter_log(0, RBR_RAW_LOG, USE_TIMESTAMP, "tick: %" PRIu64 ", rtc: %s, line: %.*s\n",
               uptimeGetMs(), rtcTimeBuffer, read_len, s);
  }
  spotter_log_console(0, "rbr | tick: %" PRIu64 ", rtc: %s, line: %.*s", uptimeGetMs(), rtcTimeBuffer,
            read_len, s);
  printf("rbr | tick: %" PRIu64 ", rtc: %s, line: %.*s\n", uptimeGetMs(), rtcTimeBuffer,
         read_len, s);

  // Use LineParser to turn string data into numeric values.
  if (_parsers[_type] && _parsers[_type]->parseLine(s, read_len)) {
    switch (_type) {
    case BmRbrDataMsg::SensorType::TEMPERATURE: {
      Value timeValue = _parsers[_type]->getValue(0);
      Value tempValue = _parsers[_type]->getValue(1);
      if (timeValue.type != TYPE_UINT64 || tempValue.type != TYPE_DOUBLE) {
        printf("Parsed invalid values: time: %d, temp: %d\n", timeValue.type, tempValue.type);
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
      success = true;
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
      success = true;
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
      success = true;
      break;
    }
    default: {
      printf("Invalid rbr sensor type: %d\n", _type);
      break;
    }
    }
  } else {
    printf("Error parsing line!\n");
  }
  return success;
}

/*!
* @brief Flush the data from the sensor driver.
*/
void RbrSensor::flush(void) { PLUART::reset(); }
