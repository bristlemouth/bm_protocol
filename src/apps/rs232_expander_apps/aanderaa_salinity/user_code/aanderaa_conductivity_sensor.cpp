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
  saveConfiguration();

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
