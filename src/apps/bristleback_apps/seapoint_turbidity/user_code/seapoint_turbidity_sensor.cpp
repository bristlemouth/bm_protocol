#include "seapoint_turbidity_sensor.h"
#include "FreeRTOS.h"
#include "app_util.h"
#include "configuration.h"
#include "payload_uart.h"
#include "serial.h"
#include "spotter.h"
#include "stm32_rtc.h"
#include "task_priorities.h"
#include "uptime.h"

/**
 * @brief Initializes the Seapoint Turbidity Sensor.
 *
 * This function performs several operations to prepare the sensor for use:
 * - Asserts that the system configuration partition is available.
 * - Initializes the parser.
 * - Retrieves the sensor log enable configuration.
 * - Initializes the Payload UART (PLUART) with a specified task priority.
 * - Sets the baud rate for the PLUART to 115200, which is the rate expected by the Seapoint turbidity sensor.
 * - Disables the use of raw byte stream buffer in the PLUART.
 * - Enables the use of line buffer in the PLUART. Note that the PLUART only stores a single line at a time.
 *   If the attached payload sends lines faster than the app reads them, they will be overwritten and data will be lost.
 * - Sets a line termination character for the PLUART according to the protocol of the sensor.
 * - Enables the PLUART, effectively turning on the UART.
 */
void SeapointTurbiditySensor::init() {
  _parser.init();
  get_config_uint(BM_CFG_PARTITION_SYSTEM, SENSOR_BM_LOG_ENABLE, strlen(SENSOR_BM_LOG_ENABLE),
                  &_sensorBmLogEnable);
  printf("sensorBmLogEnable: %" PRIu32 "\n", _sensorBmLogEnable);

  PLUART::init(USER_TASK_PRIORITY);
  // Baud set to 115200, which is expected by the Seapoint turbidity sensor
  PLUART::setBaud(BAUD_RATE);
  // Disable passing raw bytes to user app.
  PLUART::setUseByteStreamBuffer(false);
  /// Warning: PLUART only stores a single line at a time. If your attached payload sends lines
  /// faster than the app reads them, they will be overwritten and data will be lost.
  PLUART::setUseLineBuffer(true);
  // Set a line termination character per protocol of the sensor.
  PLUART::setTerminationCharacter(LINE_TERM);
  // Turn on the UART.
  PLUART::enable();
}

/**
 * @brief Retrieves data from the Seapoint Turbidity Sensor.
 *
 * This function checks if a line of data is available from the sensor. If available, it reads the line into a buffer.
 * It then logs the data along with the current system uptime and RTC time.
 * The function then attempts to parse the data from the buffer. If the parsing is successful and the data is of the correct type,
 * it populates the passed BmSeapointTurbidityDataMsg::Data structure with the parsed data and the current system time.
 *
 * @param d Reference to a BmSeapointTurbidityDataMsg::Data structure where the parsed data will be stored.
 * @return Returns true if data was successfully retrieved and parsed, false otherwise.
 */
bool SeapointTurbiditySensor::getData(BmSeapointTurbidityDataMsg::Data &d) {
  bool success = false;
  if (PLUART::lineAvailable()) {
    uint16_t read_len = PLUART::readLine(_payload_buffer, sizeof(_payload_buffer));

    RTCTimeAndDate_t time_and_date = {};
    rtcGet(&time_and_date);
    char rtc_time_str[32] = {};
    rtcPrint(rtc_time_str, NULL);

    if (_sensorBmLogEnable) {
      spotter_log(0, SEAPOINT_TURBIDITY_RAW_LOG, USE_TIMESTAMP,
                 "tick: %" PRIu64 ", rtc: %s, line: %.*s\n", uptimeGetMs(), rtc_time_str,
                 read_len, _payload_buffer);
    }
    spotter_log_console(0, "turbidity | tick: %" PRIu64 ", rtc: %s, line: %.*s", uptimeGetMs(),
              rtc_time_str, read_len, _payload_buffer);
    printf("turbidity | tick: %" PRIu64 ", rtc: %s, line: %.*s\n", uptimeGetMs(), rtc_time_str,
           read_len, _payload_buffer);

    if (_parser.parseLine(_payload_buffer, read_len)) {
      Value s_signal = _parser.getValue(0);
      Value r_signal = _parser.getValue(1);
      if (s_signal.type != TYPE_DOUBLE || r_signal.type != TYPE_DOUBLE) {
        printf("Parsed invalid turbidity data: s_signal: %d, r_signal: %d\n", s_signal.type,
               r_signal.type);
      } else {
        d.header.reading_time_utc_ms = rtcGetMicroSeconds(&time_and_date) / 1000;
        d.header.reading_uptime_millis = uptimeGetMs();
        d.s_signal = s_signal.data.double_val;
        d.r_signal = r_signal.data.double_val;
        success = true;
      }
    } else {
      printf("Failed to parse turbidity data\n");
    }
  }
  return success;
}

/**
 * @brief Flushes the data from the sensor driver.
 */
void SeapointTurbiditySensor::flush(void) { PLUART::reset(); }
