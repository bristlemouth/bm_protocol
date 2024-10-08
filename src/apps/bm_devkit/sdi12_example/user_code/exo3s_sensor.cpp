//
// Created by Uma Arthika Katikapalli on 10/7/24.
// Using seapoint turbidity sensor bristleback app as a reference
//
#include "user_code.h"
#include "bm_network.h"
#include "bm_printf.h"
#include "bm_pubsub.h"
#include "bristlefin.h"
#include "bsp.h"
#include "debug.h"
#include "lwip/inet.h"
#include "payload_uart.h"
#include "sensors.h"
#include "stm32_rtc.h"
#include "task_priorities.h"
#include "uptime.h"
#include "usart.h"
#include "util.h"
#include "exo3s_sensor.h"
#include "bristlefin.h"


void SondeEXO3sSensor::init() {
  printf("Sonde Init");
//  configASSERT(systemConfigurationPartition);
//  _parser.init();
//  systemConfigurationPartition->getConfig(SENSOR_BM_LOG_ENABLE, strlen(SENSOR_BM_LOG_ENABLE),
//                                          _sensorBmLogEnable);
//  printf("sensorBmLogEnable: %" PRIu32 "\n", _sensorBmLogEnable);

  PLUART::init(USER_TASK_PRIORITY);
  PLUART::setBaud(BAUD_RATE);
  PLUART::setUseByteStreamBuffer(false);
  PLUART::setUseLineBuffer(true);
  PLUART::setTerminationCharacter(LINE_TERM);
  PLUART::enable();
}

void SondeEXO3sSensor::sdi_wake(int delay) {
  unsigned long timeStart;
  bristlefin.sdi12Tx();
  timeStart = uptimeGetMs();
  while(uptimeGetMs() < timeStart + delay);
  printf("sdi_wake\n");
//  PLUART::enable();	// re enable Serial
}


void SondeEXO3sSensor::sdi_break(void) {
  unsigned long timeStart;
  bristlefin.sdi12Tx();
  timeStart = uptimeGetMs();
  while(uptimeGetMs() < timeStart + breakTimeMin);
  printf("sdi_break\n");
//  PLUART::enable();	// re enable Serial
  flush();
}


void SondeEXO3sSensor::sdi_transmit(char* ptr) {
//  int bytesW;
  printf("sdi_transmit\n");
  while(*ptr != 0){
    PLUART::write((uint8_t *)ptr, strlen(ptr));
    flush();
    ptr++;
  }
  printf("sdi_transmit_DONE\n");
//  delay(delayAfterTransmit);
}

char SondeEXO3sSensor::sdi_receive(void) {

  char payload_buffer[256];
  printf("sdi_receive\n");
  while (!PLUART::lineAvailable()){
    vTaskDelay(pdMS_TO_TICKS(5));
  }

  uint16_t read_len = PLUART::readLine(payload_buffer, sizeof(payload_buffer));
  printf("Read Line ----- %.*s\n", read_len, payload_buffer);
  printf("sdi_receive_DONE\n");
  return 0;
}

char SondeEXO3sSensor::sdi_cmd(const char *cmd) {
//  char index,cnt,i;
//  char* ptr;
//  char* xptr;
//  float fvalue[5];
  char result = sdiSuccess;

  strcpy(&txBuffer[0],cmd);

  sdi_break();	// send break
  sdi_transmit(&txBuffer[0]);
  result = sdi_receive();

  return result;
}


//bool SondeEXO3sSensor::readData(char *buffer) {
//  buffer = 0;
//  return false;
//}

/**
 * @brief Flushes the data from the sensor driver.
 */
void SondeEXO3sSensor::flush(void) { PLUART::reset(); }