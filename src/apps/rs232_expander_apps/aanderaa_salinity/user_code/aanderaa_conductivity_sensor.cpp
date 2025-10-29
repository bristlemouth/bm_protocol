//
// Created by Uma Arthika Katikapalli on 8/20/25.
//
#include "aanderaa_conductivity_sensor.h"
#include "FreeRTOS.h"
#include "app_util.h"
#include "configuration.h"
#include "payload_uart.h"
#include "serial.h"
#include "spotter.h"
#include "stm32_rtc.h"
#include "task_priorities.h"
#include "uptime.h"

#if __has_include ("debug.h")
#include "debug.h"
#else
#define debug_printf printf
#endif

void AanderaaConductivitySensor::init() {
  _parser.init();
  get_config_uint(BM_CFG_PARTITION_SYSTEM, SENSOR_BM_LOG_ENABLE, strlen(SENSOR_BM_LOG_ENABLE),
                  &_sensorBmLogEnable);
  debug_printf("sensorBmLogEnable: %" PRIu32 "\n", _sensorBmLogEnable);

  // reading period in seconds
  get_config_uint(BM_CFG_PARTITION_SYSTEM, SENSOR_INTERVAL_S, strlen(SENSOR_INTERVAL_S),
                   &_readingPeriodS);
  debug_printf("readingPeriodS: %" PRIu32 "\n", _readingPeriodS);

  // sensor depth in meters
  get_config_float(BM_CFG_PARTITION_SYSTEM, SENSOR_DEPTH_M, strlen(SENSOR_DEPTH_M),
                   &_sensorDepthM);
  debug_printf("sensorDepthM: %f\n", _sensorDepthM);
  // convert depth in meters to pressure in kPa; Pressure = 10 * depth
  _pressureKpa = _sensorDepthM * 10.0f;
  debug_printf("Calculated pressure for depth %.2f m is %f kPa\n", _sensorDepthM, _pressureKpa);

  PLUART::init(USER_TASK_PRIORITY);
  // Baud set to 9600, which is expected by the Aanderaa conductivity sensor
  PLUART::setBaud(BAUD_RATE);
  // Disable passing raw bytes to user app.
  PLUART::setUseByteStreamBuffer(true);
  PLUART::setUseLineBuffer(true);
  // Set a line termination character per protocol of the sensor.
  PLUART::setTerminationCharacter(LINE_TERM);
  // Turn on the UART.
  PLUART::enable();
}

void AanderaaConductivitySensor::configureSensor(void) {
  // takes sensor a few ms between each commands
  uint16_t read_len = 0;
  vTaskDelay(pdMS_TO_TICKS(1000));
  sendCommand(CMD_WAKE);

  // send stop command to stop streaming
  sendCommand(CMD_STOP);

  // passkey command
  sendCommand(CMD_SET_PASSKEY_1);

  // enable sleep
  sendCommand(CMD_ENABLE_SLEEP_YES);


  // set interval, define default interval
  AanderaaConductivityString interval_cmd;
  snprintf(interval_cmd, sizeof(interval_cmd), CMD_SET_INTERVAL, _readingPeriodS);
  sendCommand(interval_cmd);

  // enable Temperature
  sendCommand(CMD_ENABLE_TEMPERATURE_YES);

  // disable rawdata
  sendCommand(CMD_ENABLE_RAWDATA_NO);

  // disable RawCond1
  sendCommand(CMD_ENABLE_RAWCOND1_NO);

  // enable conductivity
  sendCommand(CMD_ENABLE_CONDUCTIVITY_YES);

  //  disable Polled Mode
  sendCommand(CMD_ENABLE_POLLEDMODE_NO);

  // disable text
  sendCommand(CMD_ENABLE_TEXT_NO);

  // enable decimalformat
  sendCommand(CMD_ENABLE_DECIMALFORMAT_YES);

  // enable derived parameters
  sendCommand(CMD_ENABLE_DERIVEDPARAMETERS_YES);

  // set pressure command
  debug_printf("Calculated pressure for depth %.2f m is %.2f kPa\n", _sensorDepthM, _pressureKpa);
  AanderaaConductivityString pressure_cmd;
  snprintf(pressure_cmd, sizeof(pressure_cmd), CMD_SET_PRESSURE, _pressureKpa);
  sendCommand(pressure_cmd);

  // save
  sendCommand(CMD_SAVE, _saveTimeMs);

  // send get_all command
  PLUART::write((uint8_t *)CMD_GET_ALL, strlen(CMD_GET_ALL));
  uint32_t read_duration_ms = 1000;
  uint32_t start_time = pdTICKS_TO_MS(xTaskGetTickCount());
  while ((pdTICKS_TO_MS(xTaskGetTickCount()) - start_time) < read_duration_ms) {
    if (PLUART::lineAvailable()) {
      read_len = PLUART::readLine(_payload_buffer, sizeof(_payload_buffer));
      if (read_len > 0) {
        debug_printf("%.*s\n", read_len, _payload_buffer);
        if (_sensorBmLogEnable) {
          spotter_log(0, AANDERAA_CONDUCTIVITY_RAW_LOG, USE_TIMESTAMP,
                     "tick: %" PRIu64 ", line: %.*s\n", uptimeGetMs(), read_len, _payload_buffer);
        }
        clearPayloadBuffer();
      }
    }
  }

  clearPayloadBuffer();
  vTaskDelay(pdMS_TO_TICKS(1000));
}

void AanderaaConductivitySensor::clearPayloadBuffer(void) {
  memset(_payload_buffer, 0, sizeof(_payload_buffer));
}

void AanderaaConductivitySensor::resetSensor(void) {
  sendCommand(CMD_RESET);
}

void AanderaaConductivitySensor::startStreaming(void) {
  sendCommand(CMD_START);
}

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
    spotter_log_console(0, "salinity | tick: %" PRIu64 ", rtc: %s, line: %.*s", uptimeGetMs(),
              rtc_time_str, read_len, _payload_buffer);
    debug_printf("conductivity | tick: %" PRIu64 ", rtc: %s, line: %.*s\n", uptimeGetMs(), rtc_time_str,
           read_len, _payload_buffer);

    if (read_len > 10 && _payload_buffer[0] == 0x13 && _payload_buffer[1] == 0x11){
        // Skip the \x13\x11 header bytes
        char *data_start = &_payload_buffer[2];
        // Skip the first two tab-separated fields
        for (int i = 0; i < 2; i++) {
          data_start = strchr(data_start, '\t');
          if (data_start) {
            data_start++; // Move past the tab
          } else {
            debug_printf("Failed to find expected tabs in data\n");
            return success;
          }
        }

        if (_parser.parseLine(data_start, read_len - (data_start - _payload_buffer))) {
          Value conductivity = _parser.getValue(0);
          Value temperature = _parser.getValue(1);
          Value salinity = _parser.getValue(2);
          Value water_density = _parser.getValue(3);
          Value sound_speed = _parser.getValue(4);

          d.header.reading_time_utc_ms = rtcGetMicroSeconds(&time_and_date) / 1000;
          d.header.reading_uptime_millis = uptimeGetMs();
          d.conductivity_ms_cm = conductivity.data.double_val;
          d.temperature_deg_c = temperature.data.double_val;
          d.salinity_psu = salinity.data.double_val;
          d.water_density_kg_m3 = water_density.data.double_val;
          d.sound_speed_m_s = sound_speed.data.double_val;
          d.depth_m = _sensorDepthM;
          success = true;

          debug_printf("conductivity: %.3f mS/cm, temperature: %.3f C, salinity: %.3f PSU, water density: %.3f kg/m3, sound speed: %.3f m/s, depth: %.3f m\n", d.conductivity_ms_cm, d.temperature_deg_c, d.salinity_psu, d.water_density_kg_m3, d.sound_speed_m_s, d.depth_m);
        } else {
          debug_printf("Failed to parse 5 double values from conductivity data\n");
        }
        clearPayloadBuffer();
      }
    }
  return success;
}

void AanderaaConductivitySensor::calibrateCellCoef(void) {
  // read sys config referenceConductivity
  get_config_float(BM_CFG_PARTITION_SYSTEM, EXTERNAL_REFERENCE_CONDUCTIVITY, strlen(EXTERNAL_REFERENCE_CONDUCTIVITY), &_referenceConductivity);

  if (!isnan(_referenceConductivity)) {
    // if _referenceConductivity is within expected range --- Minimum = 0.000 S/m (0.000 mS/cm) and Maximum = 7.500 S/m (75.000 mS/cm)
    if (_referenceConductivity < 0.000f || _referenceConductivity > 75.000f) {
      debug_printf("Reference conductivity is out of range. Skipping cellCoef adjustment\n");
      return;
    } else {
      debug_printf("Reference conductivity %f mS/cm is within range. Proceeding with cellCoef adjustment\n", _referenceConductivity);
	  clearPayloadBuffer();
	  uint16_t read_len = 0;
      PLUART::write((uint8_t *)"\r\n", strlen("\r\n"));
      vTaskDelay(pdMS_TO_TICKS(500));

      PLUART::write((uint8_t *)CMD_SET_PASSKEY_1000, strlen(CMD_SET_PASSKEY_1000));
	  if (PLUART::lineAvailable()) {
      	read_len = PLUART::readLine(_payload_buffer, sizeof(_payload_buffer));
	  	if (read_len > 0) {
        	debug_printf("Read line for passkey 1000: %.*s\n", read_len, _payload_buffer);
     	 }
	  }
      vTaskDelay(pdMS_TO_TICKS(500));

      // read cellCoef
      sendCommand(CMD_GET_CELL_COEF, &_cellCoef);

      // read conductivity
      sendCommand(CMD_GET_CONDUCTIVITY, &_measuredConductivity);

      if (_cellCoef == 0.000000f || _measuredConductivity == 0.000f) {
        /* calibration failed but the loop() in user_code.cpp will run this function again to
         * attempt doing it again and then remove referenceConductivity from the configs */
        debug_printf("Calibration failed because cellCoef or measuredConductivity is zero\n");
        return;
      } else {
      // calculate new cellCoef
        debug_printf("old cellCoef: %f\n", _cellCoef);
        // Formula -> NEW cellCoef = stored cellCoef * (referenceConductivity / measuredConductivity)
        _cellCoef = _cellCoef * (_referenceConductivity / _measuredConductivity);
        debug_printf("new cellCoef: %f\n", _cellCoef);

        // write new cellCoef to sensor
        char calibrate_cmd[32];
        snprintf(calibrate_cmd, sizeof(calibrate_cmd), CMD_SET_CELL_COEF, _cellCoef);
        PLUART::write((uint8_t *)calibrate_cmd, strlen(calibrate_cmd));

        // reset sensor
        resetSensor();
        // remove referenceConductivity from the config
        remove_key(BM_CFG_PARTITION_SYSTEM, EXTERNAL_REFERENCE_CONDUCTIVITY, strlen(EXTERNAL_REFERENCE_CONDUCTIVITY));
        save_config(BM_CFG_PARTITION_SYSTEM, true);
      }
    }
  } 
}

/*!
 @brief Parse And Assign Unsigned Integer Value From Sensor Output String

 @param output the output string from the sensor to parse
 @param length the length of the output string
 @param value pointer to store the parsed unsigned integer value
 */
void AanderaaConductivitySensor::checkTypeAndAssign(const char *output,
    uint16_t length,
    AanderaaConductivitySensor::AanderaaConductivityUint *value) {
  (void)length;

  if (value)  {
    *value = (uint32_t)strtoul(output, NULL, 10);
  }
}

/*!
 @brief Parse And Assign Float Value From Sensor Output String

 @param output the output string from the sensor to parse
 @param length unused
 @param value pointer to store the parsed float value
 */
void AanderaaConductivitySensor::checkTypeAndAssign(
    const char *output,
    uint16_t length,
    AanderaaConductivitySensor::AanderaaConductivityFloat *value) {
  (void)length;

  if (value)  {
    *value = strtof(output, NULL);
  }
}

/*!
 @brief Copy And Assign String Value From Sensor Output String

 @param output the output string from the sensor to copy
 @param length the length of the output string
 @param value pointer to string buffer to store the copied string
 */
void AanderaaConductivitySensor::checkTypeAndAssign(
    const char *output,
    uint16_t length,
    AanderaaConductivitySensor::AanderaaConductivityString *value) {
  if (value)  {
    size_t copy_len = bm_min(length, sizeof(AanderaaConductivityString) - 1);
    strncpy(*value, output, copy_len);
    (*value)[copy_len] = '\0';
  }
}

/*!
 @brief Send Command To Aanderaa Sensor Without Retrieving Response Value

 @details Sends a command string to the Aanderaa sensor via UART and waits for
          an acknowledgment response. This is a convenience wrapper for commands
          that don't need to retrieve a value.

 @param command the command string to send to the sensor
 @param timeout_ms timeout in milliseconds to wait for sensor acknowledgment

 @return BmOK on success
 @return BmEINVAL if command is NULL or timeout_ms is 0
 @return BmETIMEDOUT if no response received within timeout
 @return BmEBADMSG if sensor responds with error acknowledgment
 */
BmErr AanderaaConductivitySensor::sendCommand(const char *command, uint32_t timeout_ms) {
  return sendCommand(command, static_cast<uint32_t *>(nullptr), timeout_ms);
}

/*!
 @brief Send Command To Aanderaa Sensor And Optionally Retrieve Response Value

 @details Sends a command string to the Aanderaa sensor via UART and waits for
          an acknowledgment response. If a value pointer is provided, this function
          will parse the sensor's response and extract the value after the last tab
          character. The function waits byte-by-byte for responses, handling both
          simple acknowledgments, and full data responses with values.
          Acknowledgment codes follow TD321 Operation Manual section 5.5.

 @param command the command string to send to the sensor
 @param value pointer to store the parsed response value (can be NULL if no value needed)
 @param timeout_ms timeout in milliseconds to wait for sensor acknowledgment

 @return BmOK on success
 @return BmEINVAL if command is NULL or timeout_ms is 0
 @return BmETIMEDOUT if no response received within timeout
 @return BmEBADMSG if sensor responds with error acknowledgment ('*')
 */
template <typename T>
BmErr AanderaaConductivitySensor::sendCommand(const char *command,
                                              T *value,
                                              uint32_t timeout_ms) {
  BmErr err = BmEINVAL;

  if (!command || !timeout_ms) {
    return err;
  }

  constexpr uint8_t wait_read_tick = pdMS_TO_TICKS(1);
  PLUART::flush();
  clearPayloadBuffer();
  uint32_t start_time = uptimeGetMs();
  err = BmETIMEDOUT;
  uint16_t buf_idx = 0;

  debug_printf("command: %s", command);
  // Send the command and wait for acknowledgement
  PLUART::write((uint8_t *)command, strlen(command));
  while ((uptimeGetMs() - start_time) < timeout_ms) {
    if (!PLUART::byteAvailable()) {
      // Delay for UART task to process incoming bytes
      vTaskDelay(wait_read_tick);
      continue;
    }

    _payload_buffer[buf_idx] = PLUART::readByte();
    // Some responses do not finish with a new line such as '!' and '%'
    // see section 5.4 in TD321 Operation Manual
    if (!buf_idx) {
      if (_payload_buffer[buf_idx] == '!') {
        err = BmOK;
        break;
      } 
    }
    
    if (_payload_buffer[buf_idx] == '\n') {
      debug_printf("command response: %.*s\n", buf_idx, _payload_buffer);

      // read value after 3rd tab if it is a get command
      char *last_tab = strrchr(_payload_buffer, '\t');
      if (last_tab != NULL && value) {
        // +1 to skip the tab
        last_tab++;
        last_tab[strcspn(last_tab, "\r\n")] = '\0';
        checkTypeAndAssign(last_tab, strlen(last_tab), value);
      } 

      // Acknowledge for message reports '*' followed by a string for a failure and
      // '#' for success, see section 5.5 in TD321 Operation Manual
      // Sometimes there is junk before # on CMD_SAVE, the ack_idx accounts for that
      uint8_t ack_idx = buf_idx > sizeof("\r\n") ? buf_idx - 2 : 0;
      if (_payload_buffer[ack_idx] == '#') {
        err = BmOK;
        break;
      } else if (_payload_buffer[0] == '*') {
        err = BmEBADMSG;
        break;
      }

      buf_idx = 0;
    } else {
      buf_idx++;
    }
  }

  debug_printf("%s err: %d\n", __func__, err);

  return err;
}
