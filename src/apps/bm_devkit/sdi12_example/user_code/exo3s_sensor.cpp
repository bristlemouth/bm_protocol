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
#include "stm32_io.h"


void SondeEXO3sSensor::init() {
  printf("SDI-12 Initialization");
  PLUART::init(USER_TASK_PRIORITY);
  PLUART::setBaud(BAUD_RATE);
  PLUART::setUseByteStreamBuffer(false);
  PLUART::setUseLineBuffer(true);

  /* Unable to set the parity, data width and tx inversion from this function yet
   * Changing this in the MX_LPUART1_UART_Init() in usart.c
   * Can set it here after the bug fix, hopefully
//  PLUART::setEvenParity();
//  PLUART::set7bitDatawidth();
//  PLUART::enableDataInversion();
   */

  PLUART::setTerminationCharacter(LINE_TERM);
  // startTransactions - sdi12Tx (enable TX by setting OE LOW)
  // endTransactions - sdi12Rx (disable TX by setting OE HIGH)
  PLUART::enableTransactions(Bristlefin::sdi12Tx, Bristlefin::sdi12Rx);
  PLUART::enable();
}

void SondeEXO3sSensor::sdi_wake(int delay) {
  unsigned long timeStart;
  timeStart = uptimeGetMs();
  while(uptimeGetMs() < timeStart + delay);
  printf("sdi_wake\n");
}


void SondeEXO3sSensor::sdi_break_mark(void) {
  flush();
  unsigned long timeStart;
  PLUART::disable();
  //Set TX pin to output
  PLUART::configTxPinOutput();
  // HIGH at TX pin
  PLUART::setTxPinOutputLevel();
  // Break - hold HIGH for 12 ms
  timeStart = uptimeGetMs();
  while(uptimeGetMs() < timeStart + breakTimeMin);
  // LOW at TX pin
  PLUART::resetTxPinOutputLevel();
  // Mark - gold LOW for 9 ms
  timeStart = uptimeGetMs();
  while(uptimeGetMs() < timeStart + markTimeMin);
  // Set TX pin back to TX (alternate) mode
  PLUART::configTxPinAlternate();
  // Re-enable UART
  PLUART::enable();
}


void SondeEXO3sSensor::sdi_transmit(const char *ptr) {
  // TX enable
  PLUART::startTransaction();
  sdi_break_mark();
  PLUART::write((uint8_t *)ptr, strlen(ptr));
  // TX disable
  PLUART::endTransaction(28); // 28 ms to release/get some mutex/semaphore????
}



bool SondeEXO3sSensor::sdi_receive(void) {
  uint64_t timeStart = uptimeGetMs();

  while (!PLUART::lineAvailable() && (uptimeGetMs() < timeStart + 200)){
    vTaskDelay(pdMS_TO_TICKS(5));
//    printf("stuck here\n");
  }
  if (PLUART::lineAvailable()){
    uint16_t len = PLUART::readLine(rxBuffer, sizeof(rxBuffer));
//    printf("found line \n");
    for (int i =0; i < len; i++){
      rxBuffer[i] = rxBuffer[i] & 0x7F;
    }
    rxBuffer[len] = '\0';
//    for (int i = len; i < sizeof(rxBuffer);i++){
//      rxBuffer[i] = '\0';
//    }
    printf("sdi sonde | tick: %" PRIu64 ", line: %.*s\n", uptimeGetMs(), len, rxBuffer);
    return true;
  }
  return false;
}


void SondeEXO3sSensor::sdi_cmd(int cmd) {
  char result = sdiSuccess;
//  const char *cmd;
//  sdi_transmit(cmd);
//  result = sdi_receive();
//  if(result) {
//    printf("received data: %.*s\n", sizeof(rxBuffer), rxBuffer);
//  }
//  return result;

  switch (cmd) {
    case 0:
      //0! or ?!
      printf("Inquiry command \n");
      sdi_transmit("0!");
      result = sdi_receive();
      if(result) {
        printf("received data: %.*s\n", sizeof(rxBuffer), rxBuffer);
      }
      /* Remove the command from the received str and
       * save value of slave address */
      break;
    case 1:
      // 0I!
      printf("Identification command \n");
      sdi_transmit("0I!");
      result = sdi_receive();
      if(result) {
        printf("received data: %.*s\n", sizeof(rxBuffer), rxBuffer);
      }
      /* Remove the command from the received str and
       * parse out the Identification strings */
      break;
    case 2:
      // send 2 - 0M!
      // send 3 - 0D0!
      // send 4 - 0D1!
      // send 5 - 0D2!
      // send 6 - 0D3!
      printf("Measurement and Data commands \n");
//      sdi_transmit("0I!");
//      result = sdi_receive();
//      if(result) {
//        printf("received data: %.*s\n", sizeof(rxBuffer), rxBuffer);
//      }
      break;
  
  }
}

void SondeEXO3sSensor::inquire_cmd(void){
 // send 0 - 0! or ?!
 sdi_cmd(0);
}

void SondeEXO3sSensor::identify_cmd(void){
 // send 1 - 0I!
 sdi_cmd(1);
}

void SondeEXO3sSensor::measure_cmd(void){
  sdi_cmd(2);
  // send 2 - 0M!
  // send 3 - 0D0!
  // send 4 - 0D1!
  // send 5 - 0D2!
  // send 6 - 0D3!
}


/**
 * @brief Flushes the data from the sensor driver.
 */
void SondeEXO3sSensor::flush(void) { PLUART::reset(); }