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

  get_config_float(BM_CFG_PARTITION_SYSTEM, SENSOR_DEPTH_M, strlen(SENSOR_DEPTH_M),
                   &_sensorDepth);
  printf("sensorDepthM: %f\n", _sensorDepth);

  get_config_uint(BM_CFG_PARTITION_SYSTEM, SENSOR_INTERVAL_S, strlen(SENSOR_INTERVAL_S),
                   &_intervalS);
  printf("readingIntervalS: %" PRIu32 "\n", _intervalS);

  // convert depth in meters to pressure in kPa
  _pressureKpa = _sensorDepth * 9.81f;
  printf("Calculated pressure for depth %.2f m is %.2f kPa\n", _sensorDepth, _pressureKpa);
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
  vTaskDelay(pdMS_TO_TICKS(1000));

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

  // enable Text
  PLUART::write((uint8_t *)CMD_ENABLE_TEXT_YES, strlen(CMD_ENABLE_TEXT_YES));
  vTaskDelay(pdMS_TO_TICKS(100));

  // enable decimalformat
  PLUART::write((uint8_t *)CMD_ENABLE_DECIMALFORMAT_YES, strlen(CMD_ENABLE_DECIMALFORMAT_YES));
  vTaskDelay(pdMS_TO_TICKS(100));

  // enable derived parameters
  PLUART::write((uint8_t *)CMD_ENABLE_DERIVEDPARAMETERS_YES, strlen(CMD_ENABLE_DERIVEDPARAMETERS_YES));
  vTaskDelay(pdMS_TO_TICKS(100));

  // set pressure command if _pressureKpa is not NAN
  if (!isnan(_sensorDepth)) {
    printf("Calculated pressure for depth %.2f m is %.2f kPa\n", _sensorDepth, _pressureKpa);
    char pressure_cmd[32];
    snprintf(pressure_cmd, sizeof(pressure_cmd), CMD_SET_PRESSURE, _pressureKpa);
    PLUART::write((uint8_t *)pressure_cmd, strlen(pressure_cmd));

  } else {
    printf("sensorDepthM was set to NAN, so setting Pressure to 0.0 kpa\n");
    char pressure_cmd[32];
    snprintf(pressure_cmd, sizeof(pressure_cmd), CMD_SET_PRESSURE, (float)0.0);
    PLUART::write((uint8_t *)pressure_cmd, strlen(pressure_cmd));
  }
  vTaskDelay(pdMS_TO_TICKS(200));

  // save
  PLUART::write((uint8_t *)CMD_SAVE, strlen(CMD_SAVE));
  vTaskDelay(pdMS_TO_TICKS(6000));

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
  vTaskDelay(pdMS_TO_TICKS(2000));
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

    // Check if this is a measurement frame (starts with \x13\x11MEASUREMENT)
    if (read_len > 15 && _payload_buffer[0] == 0x13 && _payload_buffer[1] == 0x11 &&
        strncmp(&_payload_buffer[2], "MEASUREMENT", 11) == 0) {

      // Parse the measurement data using string parsing
      char *line = _payload_buffer;

      // Find and parse Conductivity[mS/cm]
      char *cond_pos = strstr(line, "Conductivity[mS/cm]");
      if (cond_pos) {
        cond_pos += strlen("Conductivity[mS/cm]");
        while (*cond_pos == ' ' || *cond_pos == '\t') cond_pos++; // Skip whitespace
        _conductivity = strtod(cond_pos, NULL);
      }

      // Find and parse Temperature[Deg.C]
      char *temp_pos = strstr(line, "Temperature[Deg.C]");
      if (temp_pos) {
        temp_pos += strlen("Temperature[Deg.C]");
        while (*temp_pos == ' ' || *temp_pos == '\t') temp_pos++; // Skip whitespace
        _temperature = strtod(temp_pos, NULL);
      }

      // Find and parse Salinity[PSU]
      char *sal_pos = strstr(line, "Salinity[PSU]");
      if (sal_pos) {
        sal_pos += strlen("Salinity[PSU]");
        while (*sal_pos == ' ' || *sal_pos == '\t') sal_pos++; // Skip whitespace
        _salinity = strtod(sal_pos, NULL);
      }

      // Find and parse Density[kg/m3]
      char *dens_pos = strstr(line, "Density[kg/m3]");
      if (dens_pos) {
        dens_pos += strlen("Density[kg/m3]");
        while (*dens_pos == ' ' || *dens_pos == '\t') dens_pos++; // Skip whitespace
        _waterdensity = strtod(dens_pos, NULL);
      }

      // Find and parse Soundspeed[m/s]
      char *sound_pos = strstr(line, "Soundspeed[m/s]");
      if (sound_pos) {
        sound_pos += strlen("Soundspeed[m/s]");
        while (*sound_pos == ' ' || *sound_pos == '\t') sound_pos++; // Skip whitespace
        _soundspeed = strtod(sound_pos, NULL);
      }

      // Populate the message structure
      d.header.reading_time_utc_ms = rtcGetMicroSeconds(&time_and_date) / 1000;
      d.header.sensor_reading_time_ms = uptimeGetMs();
      d.conductivity_ms_cm = _conductivity;
      d.temperature_deg_c = _temperature;
      d.salinity_psu = _salinity;
      d.water_density_kg_m3 = _waterdensity;
      d.sound_speed_m_s = _soundspeed;
      d.depth_m = _sensorDepth; // Use configured depth

      printf("Parsed: Cond=%.3f, Temp=%.3f, Sal=%.3f, Dens=%.3f, Sound=%.3f, Depth=%.2f\n",
             _conductivity, _temperature, _salinity, _waterdensity, _soundspeed, _sensorDepth);

      success = true;
    }
  }
  return success;
}