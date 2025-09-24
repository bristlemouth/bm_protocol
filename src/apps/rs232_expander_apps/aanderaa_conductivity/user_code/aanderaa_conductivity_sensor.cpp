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

void AanderaaConductivitySensor::init() {
  _parser.init();
  _configParser.init();
  get_config_uint(BM_CFG_PARTITION_SYSTEM, SENSOR_BM_LOG_ENABLE, strlen(SENSOR_BM_LOG_ENABLE),
                  &_sensorBmLogEnable);
  printf("sensorBmLogEnable: %" PRIu32 "\n", _sensorBmLogEnable);

  // reading period in seconds
  get_config_uint(BM_CFG_PARTITION_SYSTEM, SENSOR_INTERVAL_S, strlen(SENSOR_INTERVAL_S),
                   &_readingPeriodS);
  printf("readingPeriodS: %" PRIu32 "\n", _readingPeriodS);

  // sensor depth in meters
  get_config_float(BM_CFG_PARTITION_SYSTEM, SENSOR_DEPTH_M, strlen(SENSOR_DEPTH_M),
                   &_sensorDepthM);
  printf("sensorDepthM: %f\n", _sensorDepthM);
  // convert depth in meters to pressure in kPa; Pressure = 10 * depth
  _pressureKpa = _sensorDepthM * 10.0f;
  printf("Calculated pressure for depth %.2f m is %.2f kPa\n", _sensorDepthM, _pressureKpa);

  PLUART::init(USER_TASK_PRIORITY);
  // Baud set to 9600, which is expected by the Aanderaa conductivity sensor
  PLUART::setBaud(BAUD_RATE);
  // Disable passing raw bytes to user app.
  PLUART::setUseByteStreamBuffer(false);
  PLUART::setUseLineBuffer(true);
  // Set a line termination character per protocol of the sensor.
  PLUART::setTerminationCharacter(LINE_TERM);
  // Turn on the UART.
  PLUART::enable();
}

void AanderaaConductivitySensor::configureSensor(void) {
  // takes sensor a few ms between each commands
  uint32_t command_delay_ticks = pdMS_TO_TICKS(25);
  uint16_t read_len = 0;
  vTaskDelay(pdMS_TO_TICKS(1000));
  PLUART::write((uint8_t *)"0", strlen("0")); //wake
  vTaskDelay(pdMS_TO_TICKS(50));

  // send stop command to stop streaming
  PLUART::write((uint8_t *)CMD_STOP, strlen(CMD_STOP));
  vTaskDelay(pdMS_TO_TICKS(500));

  // passkey command
  PLUART::write((uint8_t *)CMD_SET_PASSKEY_1, strlen(CMD_SET_PASSKEY_1));
  vTaskDelay(command_delay_ticks);

  // enable sleep
  PLUART::write((uint8_t *)CMD_ENABLE_SLEEP_YES, strlen(CMD_ENABLE_SLEEP_YES));
  vTaskDelay(command_delay_ticks);

  // set interval, define default interval
  char interval_cmd[32];
  snprintf(interval_cmd, sizeof(interval_cmd), CMD_SET_INTERVAL, _readingPeriodS);
  PLUART::write((uint8_t *)interval_cmd, strlen(interval_cmd));
  vTaskDelay(command_delay_ticks);

  // enable Temperature
  PLUART::write((uint8_t *)CMD_ENABLE_TEMPERATURE_YES, strlen(CMD_ENABLE_TEMPERATURE_YES));
  vTaskDelay(command_delay_ticks);

  // disable rawdata
  PLUART::write((uint8_t *)CMD_ENABLE_RAWDATA_NO, strlen(CMD_ENABLE_RAWDATA_NO));
  vTaskDelay(command_delay_ticks);

  // disable RawCond1
  PLUART::write((uint8_t *)CMD_ENABLE_RAWCOND1_NO, strlen(CMD_ENABLE_RAWCOND1_NO));
  vTaskDelay(command_delay_ticks);

  // enable conductivity
  PLUART::write((uint8_t *)CMD_ENABLE_CONDUCTIVITY_YES, strlen(CMD_ENABLE_CONDUCTIVITY_YES));
  vTaskDelay(command_delay_ticks);

  //  disable Polled Mode
  PLUART::write((uint8_t *)CMD_ENABLE_POLLEDMODE_NO, strlen(CMD_ENABLE_POLLEDMODE_NO));
  vTaskDelay(command_delay_ticks);

  // disable text
  PLUART::write((uint8_t *)CMD_ENABLE_TEXT_NO, strlen(CMD_ENABLE_TEXT_NO));
  vTaskDelay(command_delay_ticks);

  // enable decimalformat
  PLUART::write((uint8_t *)CMD_ENABLE_DECIMALFORMAT_YES, strlen(CMD_ENABLE_DECIMALFORMAT_YES));
  vTaskDelay(command_delay_ticks);

  // enable derived parameters
  PLUART::write((uint8_t *)CMD_ENABLE_DERIVEDPARAMETERS_YES, strlen(CMD_ENABLE_DERIVEDPARAMETERS_YES));
  vTaskDelay(command_delay_ticks);

  // set pressure command
  printf("Calculated pressure for depth %.2f m is %.2f kPa\n", _sensorDepthM, _pressureKpa);
  char pressure_cmd[32];
  snprintf(pressure_cmd, sizeof(pressure_cmd), CMD_SET_PRESSURE, _pressureKpa);
  PLUART::write((uint8_t *)pressure_cmd, strlen(pressure_cmd));
  vTaskDelay(command_delay_ticks);

  // save
  PLUART::write((uint8_t *)CMD_SAVE, strlen(CMD_SAVE));
  vTaskDelay(pdMS_TO_TICKS(8000)); // needs about 7 seconds to save successfully

  // send get_all command
  PLUART::write((uint8_t *)CMD_GET_ALL, strlen(CMD_GET_ALL));
  uint32_t read_duration_ms = 1000;
  uint32_t start_time = pdTICKS_TO_MS(xTaskGetTickCount());
  while ((pdTICKS_TO_MS(xTaskGetTickCount()) - start_time) < read_duration_ms) {
    if (PLUART::lineAvailable()) {
      read_len = PLUART::readLine(_payload_buffer, sizeof(_payload_buffer));
      if (read_len > 0) {
        printf("%.*s\n", read_len, _payload_buffer);
        clearPayloadBuffer();
      }
    }
  }

  clearPayloadBuffer();
  vTaskDelay(pdMS_TO_TICKS(1000));
}

void AanderaaConductivitySensor::flush(void) {
  PLUART::reset();
}

void AanderaaConductivitySensor::clearPayloadBuffer(void) {
  memset(_payload_buffer, 0, sizeof(_payload_buffer));
}

void AanderaaConductivitySensor::resetSensor(void) {
  PLUART::write((uint8_t *)CMD_RESET, strlen(CMD_RESET));
}

void AanderaaConductivitySensor::startStreaming(void) {
  PLUART::write((uint8_t *)CMD_START, strlen(CMD_START));
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
    spotter_log_console(0, "conductivity | tick: %" PRIu64 ", rtc: %s, line: %.*s", uptimeGetMs(),
              rtc_time_str, read_len, _payload_buffer);
    printf("conductivity | tick: %" PRIu64 ", rtc: %s, line: %.*s\n", uptimeGetMs(), rtc_time_str,
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
            printf("Failed to find expected tabs in data\n");
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

          printf("conductivity: %.3f mS/cm, temperature: %.3f C, salinity: %.3f PSU, water density: %.3f kg/m3, sound speed: %.3f m/s, depth: %.3f m\n", d.conductivity_ms_cm, d.temperature_deg_c, d.salinity_psu, d.water_density_kg_m3, d.sound_speed_m_s, d.depth_m);
        } else {
          printf("Failed to parse 5 double values from conductivity data\n");
        }
        clearPayloadBuffer();
      }
    }
  return success;
}

void AanderaaConductivitySensor::calibrateCellCoef(void) {
  // read sys config referenceConductivity
  get_config_float(BM_CFG_PARTITION_SYSTEM, EXTERNAL_REFERENCE_CONDUCTIVITY, strlen(EXTERNAL_REFERENCE_CONDUCTIVITY),
                   &_referenceConductivity);

  // if referenceConductivity is not NAN, continue and perform cellCoef adjustment
  if (!isnan(_referenceConductivity)) {
    // if _referenceConductivity is within expected range --- Minimum = 0.000 S/m (0.000 mS/cm) and Maximum = 7.500 S/m (75.000 mS/cm)
    if (_referenceConductivity < 0.000f || _referenceConductivity > 75.000f) {
      printf("Reference conductivity is out of range. Skipping cellCoef adjustment\n");
      return;
    } else {
      printf("Reference conductivity %f mS/cm is within range. Proceeding with cellCoef adjustment\n", _referenceConductivity);
      // char calibrate_cmd[32];
      PLUART::write((uint8_t *)"\r\n", strlen("\r\n"));
      vTaskDelay(pdMS_TO_TICKS(500));
      // Set passkey(1000)
      PLUART::write((uint8_t *)CMD_SET_PASSKEY_1000, strlen(CMD_SET_PASSKEY_1000));
      vTaskDelay(pdMS_TO_TICKS(500));
      // read cellCoef from the sensor
      PLUART::write((uint8_t *)CMD_GET_CELL_COEF, strlen(CMD_GET_CELL_COEF));
      vTaskDelay(pdMS_TO_TICKS(5));
      if (PLUART::lineAvailable()) {
        // cellCoef\t5990\t67\t4.585078E+00
        uint16_t read_len = PLUART::readLine(_payload_buffer, sizeof(_payload_buffer));
        if (read_len > 0 && _configParser.parseLine(_payload_buffer, read_len)) {
          Value cellCoefValue = _configParser.getValue(3);
          _cellCoef = cellCoefValue.data.double_val;
          printf("Cell Coefficient: %f\n", _cellCoef);
        }
      }
      vTaskDelay(pdMS_TO_TICKS(5000)); //debug delay
      // read conductivity
      PLUART::write((uint8_t *)CMD_GET_CONDUCTIVITY, strlen(CMD_GET_CONDUCTIVITY));
      vTaskDelay(pdMS_TO_TICKS(5));
      if (PLUART::lineAvailable()) {
        // Conductivity[mS/cm]\t5990\t67\t0.001
        uint16_t read_len = PLUART::readLine(_payload_buffer, sizeof(_payload_buffer));
        if (read_len > 0 && _configParser.parseLine(_payload_buffer, read_len)) {
          Value conductivityValue = _configParser.getValue(3);
          _measuredConductivity = conductivityValue.data.double_val;
          printf("Measured Conductivity: %f\n", _measuredConductivity);
        }
      }
      vTaskDelay(pdMS_TO_TICKS(500));

      // calculate new cellCoef
      printf("old cellCoef: %f\n", _cellCoef);
      // Formula -> NEW cellCoef = stored cellCoef * (referenceConductivity / measuredConductivity)
      _cellCoef = _cellCoef * (_referenceConductivity / _measuredConductivity); 
      printf("new cellCoef: %f\n", _cellCoef);

      // write new cellCoef, wanna wait for while.. don't want to delete yet
      // snprintf(calibrate_cmd, sizeof(calibrate_cmd), CMD_CALIBRATE_CELL_COEF, _cellCoef);
      // PLUART::write((uint8_t *)calibrate_cmd, strlen(calibrate_cmd));

      // reset sensor
      resetSensor();
      // remove referenceConductivity from the config
      remove_key(BM_CFG_PARTITION_SYSTEM, EXTERNAL_REFERENCE_CONDUCTIVITY, strlen(EXTERNAL_REFERENCE_CONDUCTIVITY));
      save_config(BM_CFG_PARTITION_SYSTEM, true);
    }
  }
}
