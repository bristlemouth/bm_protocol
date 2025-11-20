//
// Created by Uma Arthika Katikapalli on 8/20/25.
//
#include "aanderaa_conductivity_sensor.h"
#include "FreeRTOS.h"
#include "app_util.h"
#include "configuration.h"
#include "payload_uart.h"
#include "spotter.h"
#include "stm32_rtc.h"
#include "task_priorities.h"
#include "uptime.h"
#include <string>

extern "C" {
#include "bm_rtc.h"
}
#if __has_include("debug.h")
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
  sendCommand(CMD_SET_PASSKEY_1000);

  // enable sleep
  readValidateWriteValue(CMD_ENABLE_SLEEP, CMD_YES);

  // set sleep timeout to 10s
  readValidateWriteValue(CMD_COMM_TIMEOUT, "10 s");

  checkAssignProductionConfigs();

  get_config_uint(BM_CFG_PARTITION_SYSTEM, SENSOR_SERIAL_NUMBER, strlen(SENSOR_SERIAL_NUMBER),
                  &_serialNumber);

  // set lower priveledge level
  sendCommand(CMD_SET_PASSKEY_1);

  // set interval, define default interval
  readValidateWriteValue(CMD_INTERVAL, (AanderaaConductivityFloat)_readingPeriodS);

  // enable Temperature
  readValidateWriteValue(CMD_ENABLE_TEMPERATURE, CMD_YES);

  // disable rawdata
  readValidateWriteValue(CMD_ENABLE_RAWDATA, CMD_NO);

  // disable RawCond1
  readValidateWriteValue(CMD_ENABLE_RAWCOND1, CMD_NO);

  // enable conductivity
  readValidateWriteValue(CMD_ENABLE_CONDUCTIVITY, CMD_YES);

  //  disable Polled Mode
  readValidateWriteValue(CMD_ENABLE_POLLEDMODE, CMD_NO);

  // disable text
  readValidateWriteValue(CMD_ENABLE_TEXT, CMD_NO);

  // enable decimalformat
  readValidateWriteValue(CMD_ENABLE_DECIMALFORMAT, CMD_YES);

  // enable derived parameters
  readValidateWriteValue(CMD_ENABLE_DERIVEDPARAMETERS, CMD_YES);

  // set pressure command
  spotter_log(0, AANDERAA_CONDUCTIVITY_RAW_LOG, USE_TIMESTAMP,
              "Calculated pressure for depth %.2f m is %f kPa\n", _sensorDepthM, _pressureKpa);
  readValidateWriteValue(CMD_PRESSURE, (AanderaaConductivityFloat)_pressureKpa);

  // save
  if (_sensorConfigDirty) {
    sendCommand(CMD_SAVE, _saveTimeMs);
    _sensorConfigDirty = false;
  }

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
                      "tick: %" PRIu64 ", line: %.*s\n", uptimeGetMs(), read_len,
                      _payload_buffer);
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

void AanderaaConductivitySensor::resetSensor(void) { sendCommand(CMD_RESET); }

void AanderaaConductivitySensor::startStreaming(void) { sendCommand(CMD_START); }

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
    debug_printf("conductivity | tick: %" PRIu64 ", rtc: %s, line: %.*s\n", uptimeGetMs(),
                 rtc_time_str, read_len, _payload_buffer);

    if (read_len > 10 && _payload_buffer[0] == 0x13 && _payload_buffer[1] == 0x11) {
      // Skip the \x13\x11 header bytes
      char *data_start = &_payload_buffer[2];
      // Skip the first two tab-separated fields
      for (int i = 0; i < 2; i++) {
        data_start = strchr(data_start, '\t');
        if (i == 0) {
          validateSerialNumber(data_start);
        }
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

        debug_printf("conductivity: %.3f mS/cm, temperature: %.3f C, salinity: %.3f PSU, water "
                     "density: %.3f kg/m3, sound speed: %.3f m/s, depth: %.3f m\n",
                     d.conductivity_ms_cm, d.temperature_deg_c, d.salinity_psu,
                     d.water_density_kg_m3, d.sound_speed_m_s, d.depth_m);
      } else {
        debug_printf("Failed to parse 5 double values from conductivity data\n");
      }
      clearPayloadBuffer();
    }
  }
  return success;
}

/*!
 @brief Calibrate the Aanderaa 5990

 @details This API is expected to be polled after the 5990 has been
          configured. It monitors the referenceConductivity configuration
          parameter. Once set the calibration process begins. This process
          invokes the following steps:
            1. Wakes up the 5990
            2. Stops the 5990 from reporting data
            3. Sets the passkey to 1000
            4. Reads the current cellCoef and conductivity reading on the 5990
            5. Calculates the updated cellCoef value
            6. Writes the updated cellCoef value to the sensor
            7. Saves the configuration on the device
            8. Sets the calibration related configuration parameters on the mote
            9. Resets the 5990
            10. Saves the system configuration partition on the mote and reboots
 */
void AanderaaConductivitySensor::calibrateCellCoef(void) {
  // read sys config referenceConductivity
  get_config_float(BM_CFG_PARTITION_SYSTEM, EXTERNAL_REFERENCE_CONDUCTIVITY,
                   strlen(EXTERNAL_REFERENCE_CONDUCTIVITY), &_referenceConductivity);

  if (isnan(_referenceConductivity)) {
    return;
  }

  spotter_log(0, AANDERAA_CONDUCTIVITY_RAW_LOG, USE_TIMESTAMP,
              "Calibrating salinity sensor...\n");

  // if _referenceConductivity is within expected range --- Minimum = 0.000 S/m (0.000 mS/cm) and Maximum = 7.500 S/m (75.000 mS/cm)
  if (_referenceConductivity < 0.000f || _referenceConductivity > 75.000f) {
    spotter_log(0, AANDERAA_CONDUCTIVITY_RAW_LOG, USE_TIMESTAMP,
                "Reference conductivity is out of range. Skipping cellCoef adjustment\n");
    return;
  }

  uint32_t calib_count = 0;
  get_config_uint(BM_CFG_PARTITION_SYSTEM, CAL_COUNT, strlen(CAL_COUNT), &calib_count);

  spotter_log(
      0, AANDERAA_CONDUCTIVITY_RAW_LOG, USE_TIMESTAMP,
      "Reference conductivity %f mS/cm is within range. Proceeding with cellCoef adjustment\n",
      _referenceConductivity);

  // Wake up the sensor and stop readings
  sendCommand(CMD_WAKE);
  sendCommand(CMD_STOP);
  sendCommand(CMD_SET_PASSKEY_1000);

  // read cellCoef
  sendCommand(CMD_GET_CELL_COEF, &_cellCoef);
  // read conductivity
  sendCommand(CMD_GET_CONDUCTIVITY, &_measuredConductivity);

  if (_cellCoef == 0.000000f || _measuredConductivity == 0.000f) {
    /* calibration failed but the loop() in user_code.cpp will run this function again to
     * attempt doing it again and then remove referenceConductivity from the configs */
    spotter_log(0, AANDERAA_CONDUCTIVITY_RAW_LOG, USE_TIMESTAMP,
                "Calibration failed because cellCoef or measuredConductivity is zero\n");
  } else {
    // calculate new cellCoef

    spotter_log(0, AANDERAA_CONDUCTIVITY_RAW_LOG, USE_TIMESTAMP, "old cellCoef: %f\n",
                _cellCoef);
    // Formula -> NEW cellCoef = stored cellCoef * (referenceConductivity / measuredConductivity)
    _cellCoef = _cellCoef * (_referenceConductivity / _measuredConductivity);
    spotter_log(0, AANDERAA_CONDUCTIVITY_RAW_LOG, USE_TIMESTAMP, "new cellCoef: %f\n",
                _cellCoef);

    // write new cellCoef to sensor
    AanderaaConductivityString calibrate_cmd;
    snprintf(calibrate_cmd, sizeof(calibrate_cmd), CMD_SET_CELL_COEF, _cellCoef);
    sendCommand(calibrate_cmd);
    sendCommand(CMD_SAVE, _saveTimeMs);

    // Set calibration configuration parameters
    calib_count++;
    remove_key(BM_CFG_PARTITION_SYSTEM, LAST_CAL_TIME_EPOCH_S, strlen(LAST_CAL_TIME_EPOCH_S));
    set_config_float(BM_CFG_PARTITION_SYSTEM, CELL_COEF, strlen(CELL_COEF), _cellCoef);
    set_config_uint(BM_CFG_PARTITION_SYSTEM, CAL_COUNT, strlen(CAL_COUNT), calib_count);
    checkAssignEpochValues();

    // reset sensor
    resetSensor();
  }
  // remove referenceConductivity from the config
  remove_key(BM_CFG_PARTITION_SYSTEM, EXTERNAL_REFERENCE_CONDUCTIVITY,
             strlen(EXTERNAL_REFERENCE_CONDUCTIVITY));
  save_config(BM_CFG_PARTITION_SYSTEM, true);
}

/*!
 @brief Check and Assign Calibration Epoch Time Values

 @details Verifies the cellCoef configuration exists and the RTC time is set.
          Assigns the current RTC epoch time if calibration timestamp values
          are missing. This function sets both firstCalTimeEpochS and
          lastCalTimeEpochS timestamps.

 @see lastCalTimeEpochS
 @see firstCalTimeEpochS

 @return true if new calibration times were assigned, false otherwise
 */
bool AanderaaConductivitySensor::checkAssignEpochValues(void) {
  RtcTimeAndDate rtc_time = {};
  if (bm_rtc_get(&rtc_time) != BmOK) {
    return false;
  }

  float cell_coef;
  uint32_t first_cal_time_s, last_cal_time_s;
  bool has_calibration =
      get_config_float(BM_CFG_PARTITION_SYSTEM, CELL_COEF, strlen(CELL_COEF), &cell_coef);
  if (!has_calibration) {
    return false;
  }

  // Calibration is set, check if configurations exist for calibration time
  bool ret = false;
  uint32_t epoch_s = (uint32_t)US_TO_S(bm_rtc_get_micro_seconds(&rtc_time));
  bool has_first_cal_time = get_config_uint(BM_CFG_PARTITION_SYSTEM, FIRST_CAL_TIME_EPOCH_S,
                                            strlen(FIRST_CAL_TIME_EPOCH_S), &first_cal_time_s);
  bool has_last_cal_time = get_config_uint(BM_CFG_PARTITION_SYSTEM, LAST_CAL_TIME_EPOCH_S,
                                           strlen(LAST_CAL_TIME_EPOCH_S), &last_cal_time_s);

  if (!has_first_cal_time) {
    debug_printf("Setting first cal time %" PRIu32 "\n", epoch_s);
    set_config_uint(BM_CFG_PARTITION_SYSTEM, FIRST_CAL_TIME_EPOCH_S,
                    strlen(FIRST_CAL_TIME_EPOCH_S), epoch_s);
    ret = true;
    save_config(BM_CFG_PARTITION_SYSTEM, false);
  }

  if (!has_last_cal_time) {
    debug_printf("Setting last cal time %" PRIu32 "\n", epoch_s);
    set_config_uint(BM_CFG_PARTITION_SYSTEM, LAST_CAL_TIME_EPOCH_S,
                    strlen(LAST_CAL_TIME_EPOCH_S), epoch_s);
    ret = true;
    save_config(BM_CFG_PARTITION_SYSTEM, false);
  }

  return ret;
}

/*!
 @brief Validate Sensor Serial Number Against Stored Production Serial Number

 @details Compares the detected serial number from sensor data with the stored
          production serial number (sensorSerialNum). If a mismatch is detected,
          sets errDetectedSensorSerialNum. If the serial numbers match, removes
          the configuration parameter errDetectedSensorSerialNum.

 @see sensorSerialNum
 @see errDetectedSensorSerialNum

 @param str pointer to string containing the serial number to validate
 */
void AanderaaConductivitySensor::validateSerialNumber(const char *str) {
  AanderaaConductivityUint detected_serial_number = 0;
  uint32_t serial_number_err = 0;

  if (!str) {
    return;
  }

  checkTypeAndAssign(str, strlen(str), &detected_serial_number);

  get_config_uint(BM_CFG_PARTITION_SYSTEM, ERR_SERIAL_NUM, strlen(ERR_SERIAL_NUM),
                  &serial_number_err);
  bool serial_numbers_match = detected_serial_number == _serialNumber;

  // Only set and log the key if it has not been set yet or if the config does not exist
  if ((!_serialNumberErrSet || !serial_number_err) && !serial_numbers_match) {
    spotter_log(0, AANDERAA_CONDUCTIVITY_LOG, USE_TIMESTAMP,
                "Err: Detected 5990 serial number %" PRIu32 " does not match"
                " production serial number: %" PRIu32 "\n",
                detected_serial_number, _serialNumber);
    set_config_uint(BM_CFG_PARTITION_SYSTEM, ERR_SERIAL_NUM, strlen(ERR_SERIAL_NUM), 1);
    _serialNumberErrSet = true;
  } else if (serial_numbers_match) {
    remove_key(BM_CFG_PARTITION_SYSTEM, ERR_SERIAL_NUM, strlen(ERR_SERIAL_NUM));
    _serialNumberErrSet = false;
  }
}

/*!
 @brief Check if Production Configuration Values Exist and Populate Output Structure

 @param production_configs structure to populate with production config values

 @return true if both production configurations exist, false otherwise
 */
bool AanderaaConductivitySensor::hasProductionConfigs(ProductionConfigs &production_configs) {
  bool has_serial_number =
      get_config_uint(BM_CFG_PARTITION_SYSTEM, SENSOR_SERIAL_NUMBER,
                      strlen(SENSOR_SERIAL_NUMBER), &production_configs.serial_number);
  bool has_cell_coef =
      get_config_float(BM_CFG_PARTITION_SYSTEM, FACTORY_CELL_COEF, strlen(FACTORY_CELL_COEF),
                       &production_configs.cell_coef);

  return has_serial_number && has_cell_coef;
}

/*!
 @brief Check and Assign Production Configuration Values from Sensor

 @details Reads the serial number and cell coefficient directly from the sensor
          hardware and stores them as production configuration values if they don't
          already exist. Uses a loop to ensure multiple readings are the
          same in a row to reduce the chance of a glitch on the UART line from
          ruining the reading. The serial number is also cached in the
          _serialNumber member variable.

 @see factoryCellCoef
 @see sensorSerialNum
 */
void AanderaaConductivitySensor::checkAssignProductionConfigs(void) {
  ProductionConfigs production_configs = {};
  ProductionConfigs prev_read_configs = {};
  ProductionConfigs read_configs = {};

  if (hasProductionConfigs(production_configs)) {
    return;
  }

  // Ensure robust readings for serial number
  do {
    prev_read_configs = read_configs;
    if (sendCommand(CMD_GET_SERIAL_NUMBER, &read_configs.serial_number) == BmOK) {
      debug_printf("Serial number is %" PRIu32 "\n", read_configs.serial_number);
    }
    if (sendCommand(CMD_GET_CELL_COEF, &read_configs.cell_coef) == BmOK) {

      debug_printf("cellCoef is %0.4f\n", read_configs.cell_coef);
    }
  } while (prev_read_configs.serial_number != read_configs.serial_number &&
           prev_read_configs.cell_coef != read_configs.cell_coef);

  debug_printf("Saving production configs!\n");
  set_config_uint(BM_CFG_PARTITION_SYSTEM, SENSOR_SERIAL_NUMBER, strlen(SENSOR_SERIAL_NUMBER),
                  read_configs.serial_number);
  set_config_float(BM_CFG_PARTITION_SYSTEM, FACTORY_CELL_COEF, strlen(FACTORY_CELL_COEF),
                   read_configs.cell_coef);
  save_config(BM_CFG_PARTITION_SYSTEM, false);
}

/*!
 @brief Parse And Assign Unsigned Integer Value From Sensor Output String

 @param output the output string from the sensor to parse
 @param length the length of the output string
 @param value pointer to store the parsed unsigned integer value
 */
void AanderaaConductivitySensor::checkTypeAndAssign(
    const char *output, uint16_t length,
    AanderaaConductivitySensor::AanderaaConductivityUint *value) {
  (void)length;

  if (value) {
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
    const char *output, uint16_t length,
    AanderaaConductivitySensor::AanderaaConductivityFloat *value) {
  (void)length;

  if (value) {
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
    const char *output, uint16_t length,
    AanderaaConductivitySensor::AanderaaConductivityString *value) {
  if (value) {
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
BmErr AanderaaConductivitySensor::sendCommand(const char *command, T *value,
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

/*!
 @brief Compares UINT Values And Populates Set Buffer To Send Values

 @details If the values for read and expected do not match, the buffer used to
          set the parameter is populated.

 @param parameter parameter to set if values do not match
 @param buf buffer to populate to send the set command
 @param read read value from 5990
 @param expected expected value

 @return true if the read value matches the expected value
         false if the read value does not match the expected value
 */
bool AanderaaConductivitySensor::compareValuesPopulateBuffer(
    const char *parameter, AanderaaConductivitySensor::AanderaaConductivityString *buf,
    const AanderaaConductivitySensor::AanderaaConductivityUint read,
    const AanderaaConductivitySensor::AanderaaConductivityUint expected) {

  if (read == expected) {
    return true;
  }

  snprintf(*buf, sizeof(AanderaaConductivityString), "%s %s(%" PRIu32 ")\r\n", CMD_SET,
           parameter, expected);
  return false;
}

/*!
 @brief Compares Float Values And Populates Set Buffer To Send Values

 @details If the values for read and expected do not match, the buffer used to
          set the parameter is populated.

 @param parameter parameter to set if values do not match
 @param buf buffer to populate to send the set command
 @param read read value from 5990
 @param expected expected value

 @return true if the read value matches the expected value
         false if the read value does not match the expected value
 */
bool AanderaaConductivitySensor::compareValuesPopulateBuffer(
    const char *parameter, AanderaaConductivitySensor::AanderaaConductivityString *buf,
    const AanderaaConductivitySensor::AanderaaConductivityFloat read,
    const AanderaaConductivitySensor::AanderaaConductivityFloat expected) {

  constexpr float epsilon = 0.0001f;
  if (fabs(read - expected) < epsilon) {
    return true;
  }

  snprintf(*buf, sizeof(AanderaaConductivityString), "%s %s(%f)\r\n", CMD_SET, parameter,
           expected);
  return false;
}

/*!
 @brief Compares String Values And Populates Set Buffer To Send Values

 @details If the values for read and expected do not match, the buffer used to
          set the parameter is populated.

 @param parameter parameter to set if values do not match
 @param buf buffer to populate to send the set command
 @param read read value from 5990
 @param expected expected value

 @return true if the read value matches the expected value
         false if the read value does not match the expected value
 */
bool AanderaaConductivitySensor::compareValuesPopulateBuffer(
    const char *parameter, AanderaaConductivitySensor::AanderaaConductivityString *buf,
    const AanderaaConductivitySensor::AanderaaConductivityString read,
    const AanderaaConductivitySensor::AanderaaConductivityString expected) {

  if (!strncmp(read, expected, sizeof(AanderaaConductivityString))) {
    return true;
  }

  snprintf(*buf, sizeof(AanderaaConductivityString), "%s %s(%s)\r\n", CMD_SET, parameter,
           expected);
  return false;
}

/*!
 @brief Overloaded Wrapper To Accept String Literals For Expected Value

 @param parameter parameter to get/set
 @param expected_val expected value from get command
 @param retries number of times to retry getting/setting the parameter

 @return BmOk on success,
         BmEINVAL if expected value is longer than AanderaaConductivityString
 */
BmErr AanderaaConductivitySensor::readValidateWriteValue(const char *parameter,
                                                         const char *expected_val,
                                                         uint8_t retries) {
  AanderaaConductivityString str_buf = {};

  if (strlen(expected_val) > sizeof(str_buf)) {
    return BmEINVAL;
  }

  strncpy(str_buf, expected_val, sizeof(str_buf));

  return readValidateWriteValue<AanderaaConductivityString>(parameter, str_buf, retries);
}

/*!
 @brief Reads/Validate/Write a Parameter On The 5990

 @details This function will get a specified parameter from the 5990, compare that
          parameter to an expected value and then proceed to write that parameter
          if the read parameter is not expected. This will then mark the 5990's 
          config parameters as dirty and will save the configuration to the
          5990 at the end of configureSensor. Retries are implemented to
          ensure that the get/set commands are able to be invoked properly.

 @param parameter parameter to get/set
 @param expected_val expected value from get command
 @param retries number of times to retry getting/setting the parameter

 @return BmOK on successful write or if the expected value matches the read value
 @return BmEINVAL if command is NULL
 @return BmETIMEDOUT if no response received within timeout
 @return BmEBADMSG if sensor responds with error acknowledgment ('*')
 */
template <typename T>
BmErr AanderaaConductivitySensor::readValidateWriteValue(const char *parameter, T expected_val,
                                                         uint8_t retries) {
  T read_val;
  AanderaaConductivityString command_buf = {};
  BmErr ret = BmOK;

  do {
    snprintf(command_buf, sizeof(command_buf), "%s %s\r\n", CMD_GET, parameter);

    ret = sendCommand(command_buf, &read_val);
    if (ret != BmOK) {
      continue;
    }

    if (compareValuesPopulateBuffer(parameter, &command_buf, read_val, expected_val)) {
      ret = BmOK;
      break;
    }

    ret = sendCommand(command_buf);

    if (ret == BmOK) {
      _sensorConfigDirty = true;
    }
  } while (ret != BmOK && retries--);

  return ret;
}
