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
  printf("One time setup Aanderaa Conductivity Sensor\n");
  vTaskDelay(pdMS_TO_TICKS(2000));
  // send stop command to stop streaming
  uint16_t read_len = 0;
  read_len = PLUART::readLine(_payload_buffer, sizeof(_payload_buffer));
  // process startup message

  vTaskDelay(pdMS_TO_TICKS(3000));
  PLUART::write((uint8_t *)"0", strlen("0")); //wake
  vTaskDelay(pdMS_TO_TICKS(50));
  read_len = PLUART::readLine(_payload_buffer, sizeof(_payload_buffer));
  clearPayloadBuffer();

  PLUART::write((uint8_t *)CMD_STOP, strlen(CMD_STOP));
  vTaskDelay(pdMS_TO_TICKS(200));
  read_len = PLUART::readLine(_payload_buffer, sizeof(_payload_buffer));
  clearPayloadBuffer();
  vTaskDelay(pdMS_TO_TICKS(500));

  // passkey command
  PLUART::write((uint8_t *)CMD_SET_PASSKEY_1, strlen(CMD_SET_PASSKEY_1));
  vTaskDelay(pdMS_TO_TICKS(1000));
  // enable sleep
  PLUART::write((uint8_t *)CMD_ENABLE_SLEEP_YES, strlen(CMD_ENABLE_SLEEP_YES));
  vTaskDelay(pdMS_TO_TICKS(1000));
  // wait for #
  // set interval, define default interval
  PLUART::write((uint8_t *)CMD_SET_INTERVAL_2, strlen(CMD_SET_INTERVAL_2));
  vTaskDelay(pdMS_TO_TICKS(1000));
  // enable Temperature
  PLUART::write((uint8_t *)CMD_ENABLE_TEMPERATURE_YES, strlen(CMD_ENABLE_TEMPERATURE_YES));
  vTaskDelay(pdMS_TO_TICKS(1000));
  // enable decimalformat
  PLUART::write((uint8_t *)CMD_ENABLE_DECIMALFORMAT_YES, strlen(CMD_ENABLE_DECIMALFORMAT_YES));
  vTaskDelay(pdMS_TO_TICKS(100));
  // disable rawdata
  PLUART::write((uint8_t *)CMD_ENABLE_RAWDATA_NO, strlen(CMD_ENABLE_RAWDATA_NO));
  vTaskDelay(pdMS_TO_TICKS(100));
  // disable RawCond1
  PLUART::write((uint8_t *)CMD_ENABLE_RAWCOND1_NO, strlen(CMD_ENABLE_RAWCOND1_NO));
  vTaskDelay(pdMS_TO_TICKS(100));
  // enable conductivity
  PLUART::write((uint8_t *)CMD_ENABLE_CONDUCTIVITY_YES,
                 strlen(CMD_ENABLE_CONDUCTIVITY_YES));
  vTaskDelay(pdMS_TO_TICKS(100));
  //  disable Polled Mode
  PLUART::write((uint8_t *)CMD_ENABLE_POLLEDMODE_NO,
                 strlen(CMD_ENABLE_POLLEDMODE_NO));
  vTaskDelay(pdMS_TO_TICKS(100));
  // enable Text
  PLUART::write((uint8_t *)CMD_ENABLE_TEXT_YES, strlen(CMD_ENABLE_TEXT_YES));
  vTaskDelay(pdMS_TO_TICKS(100));
  // enable decimalformat
  PLUART::write((uint8_t *)CMD_ENABLE_DECIMALFORMAT_YES, strlen(CMD_ENABLE_DECIMALFORMAT_YES));
  vTaskDelay(pdMS_TO_TICKS(100));
  // set pressure command if _pressureKpa is not NAN
  if (!isnan(_sensorDepth)) {
    printf("Calculated pressure for depth %.2f m is %.2f kPa\n", _sensorDepth, _pressureKpa);
    char pressure_cmd[32];
    snprintf(pressure_cmd, sizeof(pressure_cmd), "Set Pressure(%.2f)\r\n", _pressureKpa);
    PLUART::write((uint8_t *)pressure_cmd, strlen(pressure_cmd));
    vTaskDelay(pdMS_TO_TICKS(100));
    // enable derived parameters
    PLUART::write((uint8_t *)CMD_ENABLE_DERIVEDPARAMETERS_YES,
    strlen(CMD_ENABLE_DERIVEDPARAMETERS_YES));
    vTaskDelay(pdMS_TO_TICKS(100));
  }
  else {
    printf("sensorDepthM was set to NAN, so setting Pressure to 0.0 kpa and disabling Derived Parameters\n");
    PLUART::write((uint8_t *)"Set Pressure(0.0)\r\n", strlen("Set Pressure(0.0)\r\n"));
    // disabling derived parameters
    PLUART::write((uint8_t *)CMD_ENABLE_DERIVEDPARAMETERS_NO,
    strlen(CMD_ENABLE_DERIVEDPARAMETERS_NO));
    vTaskDelay(pdMS_TO_TICKS(200));
  }
  vTaskDelay(pdMS_TO_TICKS(200));
  // save
  PLUART::write((uint8_t *)CMD_SAVE, strlen(CMD_SAVE));
  vTaskDelay(pdMS_TO_TICKS(200));
  clearPayloadBuffer();
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
/*
bool AanderaaConductivitySensor::getData(AanderaaConductivityMsg::Data &d) {
  // Not used for this sensor
  return false;
}*/
void AanderaaConductivitySensor::resetSensor(void) {
  // Send reset command to the sensor
  PLUART::write((uint8_t *)CMD_RESET, strlen(CMD_RESET));
  vTaskDelay(pdMS_TO_TICKS(500));
  clearPayloadBuffer();
  printf("Sensor reset command sent.\n");
}
