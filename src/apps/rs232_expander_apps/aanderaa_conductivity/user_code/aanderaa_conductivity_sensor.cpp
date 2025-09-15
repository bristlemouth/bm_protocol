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
  get_config_uint(BM_CFG_PARTITION_SYSTEM, SENSOR_BM_LOG_ENABLE, strlen(SENSOR_BM_LOG_ENABLE),
                  &_sensorBmLogEnable);
  printf("sensorBmLogEnable: %" PRIu32 "\n", _sensorBmLogEnable);

  // reading interval in seconds
  get_config_uint(BM_CFG_PARTITION_SYSTEM, SENSOR_INTERVAL_S, strlen(SENSOR_INTERVAL_S),
                   &_intervalS);
  printf("readingIntervalS: %" PRIu32 "\n", _intervalS);

  // sensor depth in meters
  get_config_float(BM_CFG_PARTITION_SYSTEM, SENSOR_DEPTH_M, strlen(SENSOR_DEPTH_M),
                   &_sensorDepthM);
  printf("sensorDepthM: %f\n", _sensorDepthM);
  // convert depth in meters to pressure in kPa; Pressure = 10 * depth
  _pressureKpa = _sensorDepthM * 10.0f;
  printf("Calculated pressure for depth %.2f m is %.2f kPa\n", _sensorDepthM, _pressureKpa);

  // calibration coefficients [PLACE HOLDER]
  get_config_uint(BM_CFG_PARTITION_SYSTEM, SENSOR_CELL_COEFF, strlen(SENSOR_CELL_COEFF),
                   &_cellCoeff);
  printf("cellCoeff: %" PRIu32 "\n", _cellCoeff);

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
  //timeout is 60000 ms, looks like we gotta wake the senor and then send any commands after this.
  uint16_t read_len = 0;
  vTaskDelay(pdMS_TO_TICKS(3000));
  PLUART::write((uint8_t *)"0", strlen("0")); //wake
  vTaskDelay(pdMS_TO_TICKS(50));

  // send stop command to stop streaming
  PLUART::write((uint8_t *)CMD_STOP, strlen(CMD_STOP));
  vTaskDelay(pdMS_TO_TICKS(500));

  // passkey command
  PLUART::write((uint8_t *)CMD_SET_PASSKEY_1, strlen(CMD_SET_PASSKEY_1));
  vTaskDelay(pdMS_TO_TICKS(100));

  // enable sleep
  PLUART::write((uint8_t *)CMD_ENABLE_SLEEP_YES, strlen(CMD_ENABLE_SLEEP_YES));
  vTaskDelay(pdMS_TO_TICKS(100));

  // set interval, define default interval
  char interval_cmd[32];
  snprintf(interval_cmd, sizeof(interval_cmd), CMD_SET_INTERVAL, _intervalS);
  PLUART::write((uint8_t *)interval_cmd, strlen(interval_cmd));
  vTaskDelay(pdMS_TO_TICKS(100));

  // enable Temperature
  PLUART::write((uint8_t *)CMD_ENABLE_TEMPERATURE_YES, strlen(CMD_ENABLE_TEMPERATURE_YES));
  vTaskDelay(pdMS_TO_TICKS(100));

  // disable rawdata
  PLUART::write((uint8_t *)CMD_ENABLE_RAWDATA_NO, strlen(CMD_ENABLE_RAWDATA_NO));
  vTaskDelay(pdMS_TO_TICKS(100));

  // disable RawCond1
  PLUART::write((uint8_t *)CMD_ENABLE_RAWCOND1_NO, strlen(CMD_ENABLE_RAWCOND1_NO));
  vTaskDelay(pdMS_TO_TICKS(100));

  // enable conductivity
  PLUART::write((uint8_t *)CMD_ENABLE_CONDUCTIVITY_YES, strlen(CMD_ENABLE_CONDUCTIVITY_YES));
  vTaskDelay(pdMS_TO_TICKS(100));

  //  disable Polled Mode
  PLUART::write((uint8_t *)CMD_ENABLE_POLLEDMODE_NO, strlen(CMD_ENABLE_POLLEDMODE_NO));
  vTaskDelay(pdMS_TO_TICKS(100));

  // disable text
  PLUART::write((uint8_t *)CMD_ENABLE_TEXT_NO, strlen(CMD_ENABLE_TEXT_NO));
  vTaskDelay(pdMS_TO_TICKS(100));

  // enable decimalformat
  PLUART::write((uint8_t *)CMD_ENABLE_DECIMALFORMAT_YES, strlen(CMD_ENABLE_DECIMALFORMAT_YES));
  vTaskDelay(pdMS_TO_TICKS(100));

  // enable derived parameters
  PLUART::write((uint8_t *)CMD_ENABLE_DERIVEDPARAMETERS_YES, strlen(CMD_ENABLE_DERIVEDPARAMETERS_YES));
  vTaskDelay(pdMS_TO_TICKS(100));

  // set pressure command
  printf("Calculated pressure for depth %.2f m is %.2f kPa\n", _sensorDepthM, _pressureKpa);
  char pressure_cmd[32];
  snprintf(pressure_cmd, sizeof(pressure_cmd), CMD_SET_PRESSURE, _pressureKpa);
  PLUART::write((uint8_t *)pressure_cmd, strlen(pressure_cmd));
  vTaskDelay(pdMS_TO_TICKS(100));

  // save
  PLUART::write((uint8_t *)CMD_SAVE, strlen(CMD_SAVE));
  vTaskDelay(pdMS_TO_TICKS(8000));

  // send get_all command and cross check if they were saved.
  PLUART::write((uint8_t *)CMD_GET_ALL, strlen(CMD_GET_ALL));
  uint16_t line_count = 0;
  while (line_count < 25) {
    if (PLUART::lineAvailable()) {
    read_len = PLUART::readLine(_payload_buffer, sizeof(_payload_buffer));
    printf("%.*s\n", read_len, _payload_buffer);
    line_count++;
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
