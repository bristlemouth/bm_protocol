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
  _sensorDepth = NAN;

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
  vTaskDelay(pdMS_TO_TICKS(200));
  // disable rawdata
  PLUART::write((uint8_t *)CMD_ENABLE_RAWDATA_NO, strlen(CMD_ENABLE_RAWDATA_NO));
  vTaskDelay(pdMS_TO_TICKS(200));
  // enable derived parameters
  PLUART::write((uint8_t *)CMD_ENABLE_DERIVEDPARAMETERS_YES,
                 strlen(CMD_ENABLE_DERIVEDPARAMETERS_YES));
  vTaskDelay(pdMS_TO_TICKS(200));
  // disable RawCond1
  PLUART::write((uint8_t *)CMD_ENABLE_RAWCOND1_NO, strlen(CMD_ENABLE_RAWCOND1_NO));
  vTaskDelay(pdMS_TO_TICKS(200));
  // enable conductivity
  PLUART::write((uint8_t *)CMD_ENABLE_CONDUCTIVITY_YES,
                 strlen(CMD_ENABLE_CONDUCTIVITY_YES));
  vTaskDelay(pdMS_TO_TICKS(200));
  //  disable Polled Mode
  PLUART::write((uint8_t *)CMD_ENABLE_POLLEDMODE_NO,
                 strlen(CMD_ENABLE_POLLEDMODE_NO));
  vTaskDelay(pdMS_TO_TICKS(200));
  // enable Text
  PLUART::write((uint8_t *)CMD_ENABLE_TEXT_YES, strlen(CMD_ENABLE_TEXT_YES));
  vTaskDelay(pdMS_TO_TICKS(200));
  // enable decimalformat
  PLUART::write((uint8_t *)CMD_ENABLE_DECIMALFORMAT_YES, strlen(CMD_ENABLE_DECIMALFORMAT_YES));
  vTaskDelay(pdMS_TO_TICKS(200));
  // set pressure command if _pressureKpa is not NAN
  if (!isnan(_sensorDepth)) {
    char pressure_cmd[32];
    snprintf(pressure_cmd, sizeof(pressure_cmd), "Set Pressure(%.2f)\r\n", _pressureKpa);
    PLUART::write((uint8_t *)pressure_cmd, strlen(pressure_cmd));
  }
  else {
    PLUART::write((uint8_t *)"Set Pressure(0.0)\r\n", strlen("Set Pressure(0.0)\r\n"));
  }
  // save
  PLUART::write((uint8_t *)CMD_SAVE, strlen(CMD_SAVE));
  vTaskDelay(pdMS_TO_TICKS(200));
  clearPayloadBuffer();
  vTaskDelay(pdMS_TO_TICKS(6000));
  // send get_all command and cross check if they were saved.
  PLUART::write((uint8_t *)CMD_GET_ALL, strlen(CMD_GET_ALL));
  read_len = PLUART::readLine(_payload_buffer, sizeof(_payload_buffer));
  printf("Get all params response: %.*s\n", read_len, _payload_buffer);

  clearPayloadBuffer();
  vTaskDelay(pdMS_TO_TICKS(2000));
  // PLUART::write((uint8_t *)CMD_RESET, strlen(CMD_RESET));

  printf("Calculated pressure for depth %.2f m is %.2f kPa\n", _sensorDepth, _pressureKpa);

  // if _pressureKpa == NAN, then don't send any pressure command; else set pressure
	//printf("%d\n",read_len);
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