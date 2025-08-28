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
  printf("One time setup Aanderaa Conductivity Sensor\n");
  vTaskDelay(pdMS_TO_TICKS(1000));
  // send stop command to stop streaming
  uint16_t read_len = 0;
  read_len = PLUART::readLine(_payload_buffer, sizeof(_payload_buffer));
  // process startup message

  vTaskDelay(pdMS_TO_TICKS(3000));
  PLUART::write((uint8_t *)"stop\r\n", strlen("stop\r\n"));
  vTaskDelay(pdMS_TO_TICKS(200));
  read_len = PLUART::readLine(_payload_buffer, sizeof(_payload_buffer));
  printf("Received line: %.*s\n", read_len, _payload_buffer);
  // clear buffer
  vTaskDelay(pdMS_TO_TICKS(1000));
  // if _payload_buffer is not "!#\r\n"
  while (strncmp(_payload_buffer, "!#", 2) != 0) {
    printf("Sensor not responding to stop command, retrying...\n");
    PLUART::write((uint8_t *)"stop\r\n", strlen("stop\r\n"));
    vTaskDelay(pdMS_TO_TICKS(200));
    read_len = PLUART::readLine(_payload_buffer, sizeof(_payload_buffer));
    printf("Received line: %.*s\n", read_len, _payload_buffer);
  }

  // passkey command
  // wait for #
  // enable sleep
  // wait for #
  // set interval, define default interval
  // enable Temperature
  // enable decimalformat
  // disable rawdata
  // enable derived parameters
  // disable RawCond1

  // enable conductivity
  // enable/disable Polled Mode?
  // enable/disable Text
  // if _pressureKpa == NAN, then don't send any pressure command; else set pressure

  // send get_all command and cross check if they were saved.
}

void AanderaaConductivitySensor::flush(void) {
  PLUART::reset();
}
/*
bool AanderaaConductivitySensor::getData(AanderaaConductivityMsg::Data &d) {
  // Not used for this sensor
  return false;
}*/