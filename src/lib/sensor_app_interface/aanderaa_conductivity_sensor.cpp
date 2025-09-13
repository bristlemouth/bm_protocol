#include "aanderaa_conductivity_sensor.h"
#include "payload_uart.h"
#include "bsp.h"
#include "debug.h"
#include "spotter.h"
#include "stm32_rtc.h"
#include "uptime.h"
#include "util.h"
#include "task_priorities.h"
#include "configuration.h"
#include "FreeRTOS.h"
#include <string.h>

/**
 * @brief Initializes the Aanderaa Conductivity Sensor.
 *
 * This function initializes the sensor by setting up the UART communication with the specified baud rate.
 * It also initializes the line parser for processing incoming data.
 */
void AanderaaConductivitySensor::init() {
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

  _parser.init();

  // Get sensor logging configuration
  if (!get_config_uint(BM_CFG_PARTITION_SYSTEM, SENSOR_BM_LOG_ENABLE, strlen(SENSOR_BM_LOG_ENABLE), &_sensorBmLogEnable)) {
    _sensorBmLogEnable = 0;
  }
}

/**
 * @brief Retrieves data from the Aanderaa Conductivity Sensor.
 *
 * This function checks if a line of data is available from the sensor. If available, it reads the line into a buffer.
 * It then logs the data along with the current system uptime and RTC time.
 * The function then attempts to parse the data from the buffer. If the parsing is successful and the data is of the correct type,
 * it populates the passed AanderaaConductivityMsg::Data structure with the parsed data and the current system time.
 *
 * Expected data format: conductivity_ms_cm,temperature_deg_c,salinity_psu,water_density_kg_m3,sound_speed_m_s,depth_m
 *
 * @param d Reference to a AanderaaConductivityMsg::Data structure where the parsed data will be stored.
 * @return Returns true if data was successfully retrieved and parsed, false otherwise.
 */
bool AanderaaConductivitySensor::getData(AanderaaConductivityMsg::Data &d) {
  bool success = false;
  if (PLUART::lineAvailable()) {
    uint16_t read_len = PLUART::readLine(_payload_buffer, sizeof(_payload_buffer));

    RTCTimeAndDate_t time_and_date = {};
    rtcGet(&time_and_date);
    char rtc_time_str[32] = {};
    rtcPrint(rtc_time_str, NULL);

    if (_sensorBmLogEnable) {
      spotter_log(0, AANDERAA_CONDUCTIVITY_RAW_LOG, USE_TIMESTAMP,
                 "tick: %" PRIu64 ", rtc: %s, line: %.*s\n", uptimeGetMs(), rtc_time_str,
                 read_len, _payload_buffer);
    }

    printf("[aanderaa_conductivity] | tick: %" PRIu64 ", rtc: %s, line: %.*s\n",
           uptimeGetMs(), rtc_time_str, read_len, _payload_buffer);

    if (_parser.parseLine(_payload_buffer, read_len)) {
      Value conductivity = _parser.getValue(0);
      Value temperature = _parser.getValue(1);
      Value salinity = _parser.getValue(2);
      Value water_density = _parser.getValue(3);
      Value sound_speed = _parser.getValue(4);
      Value depth = _parser.getValue(5);

      if (conductivity.type != TYPE_DOUBLE || temperature.type != TYPE_DOUBLE ||
          salinity.type != TYPE_DOUBLE || water_density.type != TYPE_DOUBLE ||
          sound_speed.type != TYPE_DOUBLE || depth.type != TYPE_DOUBLE) {
        printf("Parsed invalid conductivity data: types: %d,%d,%d,%d,%d,%d\n",
               conductivity.type, temperature.type, salinity.type,
               water_density.type, sound_speed.type, depth.type);
      } else {
        d.header.reading_time_utc_ms = rtcGetMicroSeconds(&time_and_date) / 1000;
        d.header.reading_uptime_millis = uptimeGetMs();
        d.conductivity_ms_cm = conductivity.data.double_val;
        d.temperature_deg_c = temperature.data.double_val;
        d.salinity_psu = salinity.data.double_val;
        d.water_density_kg_m3 = water_density.data.double_val;
        d.sound_speed_m_s = sound_speed.data.double_val;
        d.depth_m = depth.data.double_val;
        success = true;
      }
    } else {
      printf("Failed to parse conductivity data\n");
    }
  }
  return success;
}

/**
 * @brief Flushes the data from the sensor driver.
 */
void AanderaaConductivitySensor::flush(void) {
  PLUART::reset();
}
