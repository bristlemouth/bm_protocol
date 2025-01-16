//
// Created by Uma Arthika Katikapalli on 10/7/24.
// Using seapoint turbidity sensor bristleback app as a reference
//
#include "exo3s_sensor.h"
#include "bm_printf.h"
#include "bristlefin.h"
#include "bsp.h"
#include "debug.h"
#include "lwip/inet.h"
#include "payload_uart.h"
#include "sensors.h"
#include "stm32_io.h"
#include "stm32_rtc.h"
#include "task_priorities.h"
#include "uptime.h"
#include "usart.h"
#include "user_code.h"
#include "util.h"

enum {
  SDI_ADDRESS_INQUIRY_CMD,
  SDI_INDENTIFICATION_CMD,
  SDI_START_MEASUREMENT_CMD,
};

SondeEXO3sSensor::SondeEXO3sSensor()
    : d0_parser(4, "0D0!0"), d1_parser(4, "0D1!0"), d2_parser(2, "0D2!0"), _latest_sample{} {}

void SondeEXO3sSensor::init() {
  printf("SDI-12 Initialization\n");
  d0_parser.init();
  d1_parser.init();
  d2_parser.init();

  PLUART::init(USER_TASK_PRIORITY);
  PLUART::setEvenParity(); // data width is 7 bits + 1 parity bit = 8 bits
  PLUART::enableDataInversion();
  PLUART::setBaud(BAUD_RATE);
  PLUART::setUseByteStreamBuffer(false);
  PLUART::setUseLineBuffer(true);

  PLUART::setTerminationCharacter(LINE_TERM);
  // startTransactions - sdi12Tx (enable TX by setting OE LOW)
  // endTransactions - sdi12Rx (disable TX by setting OE HIGH)
  PLUART::enableTransactions(Bristlefin::sdi12Tx, Bristlefin::sdi12Rx);
  PLUART::enable();
}

void SondeEXO3sSensor::sdi_wake(void) {
  uint32_t timeStart;
  timeStart = uptimeGetMs();
  PLUART::disable();
  //Set TX pin to output
  PLUART::configTxPinOutput();
  // HIGH at TX pin
  PLUART::setTxPinOutputLevel();
  // Wake - hold HIGH for 12 ms
  timeStart = uptimeGetMs();
  while (uptimeGetMs() - timeStart < breakTimeMs)
    ;
  // Set TX pin back to TX (alternate) mode
  PLUART::configTxPinAlternate();
  // Re-enable UART
  PLUART::enable();
  printf("sdi_wake\n");
}

void SondeEXO3sSensor::sdi_break_mark(void) {
  flush();
  uint32_t timeStart;
  PLUART::disable();
  //Set TX pin to output
  PLUART::configTxPinOutput();
  // HIGH at TX pin
  PLUART::setTxPinOutputLevel();
  // Break - hold HIGH for 12 ms
  timeStart = uptimeGetMs();
  while (uptimeGetMs() - timeStart < breakTimeMs)
    ;
  // LOW at TX pin
  PLUART::resetTxPinOutputLevel();
  // Mark - hold LOW for 9 ms
  timeStart = uptimeGetMs();
  while (uptimeGetMs() - timeStart < markTimeMs)
    ;
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
  PLUART::endTransaction(100); // 50,28 ms to release/get some mutex/semaphore????
}

bool SondeEXO3sSensor::sdi_receive(void) {
  uint64_t timeStart = uptimeGetMs();
  memset(rxBuffer, 0, sizeof(rxBuffer));
  while (!PLUART::lineAvailable() && (uptimeGetMs() - timeStart < receive_timeout)) {
    vTaskDelay(pdMS_TO_TICKS(5));
  }
  if (PLUART::lineAvailable()) {
    uint16_t len = PLUART::readLine(rxBuffer, sizeof(rxBuffer));
    for (int i = 0; i < len; i++) {
      rxBuffer[i] = rxBuffer[i] & 0x7F;
    }
    return true;
  }
  return false;
}

void SondeEXO3sSensor::sdi_cmd(int cmd) {
  bool result = false;

  switch (cmd) {
  case SDI_ADDRESS_INQUIRY_CMD:
    //0! or ?!
    printf("Inquire Sensor Address \n");
    sdi_transmit("?!");
    result = sdi_receive();
    slaveID = rxBuffer[2] - '0';
    if (result) {
      printf("Sensor Address: %d\n", slaveID);
    }
    break;

  case SDI_INDENTIFICATION_CMD:
    printf("Identification command \n");
    sdi_transmit("0I!");
    result = sdi_receive();
    if (result) {
      printf("received data: %.*s\n", sizeof(rxBuffer), rxBuffer);
    }
    /* Response --- 013YSIIWQSGEXOSND100
       * 0 sdi-12 sensor address
       * 13 sdi-12 version number
       * YSIIWQSG vendor identification
       * EXOSND sensor model
       * 100 sensor version*/
    printf("SDI-12 Version Number --- %.*s\n", static_cast<int>(2), &rxBuffer[4]);
    printf("Vendor Identification --- %.*s\n", static_cast<int>(8), &rxBuffer[6]);
    printf("Sensor Model:         --- %.*s\n", static_cast<int>(6), &rxBuffer[14]);
    printf("Sensor Version:       --- %.*s\n", static_cast<int>(3), &rxBuffer[20]);
    break;

  case SDI_START_MEASUREMENT_CMD:
    int delay = 10;
    printf("Measurement and Data commands \n");
    vTaskDelay(pdMS_TO_TICKS(100));
    sdi_transmit("0M!");
    result = sdi_receive();
    if (result) {
      printf("%.*s\n", sizeof(rxBuffer), rxBuffer);
      // get time it takes for the measurement to be done
      delay = abs((rxBuffer[5] - '0') * 10 + (rxBuffer[6] - '0'));
      printf("wait %d seconds for measurement to complete\n", delay);
    }
    /* 00629
       * Measure command
       * 0 sdi-12 sensor address
       * 062 time is seconds until data available
       * 9 number of values to expect
       * after 62 seconds, 0 will be rxd to indicate measurement is done */
    vTaskDelay(pdMS_TO_TICKS(delay * 1000)); //62 seconds delay
    result = sdi_receive();
    if (result) {
      printf("measuring done: %.*s\n", sizeof(rxBuffer), rxBuffer);
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
    sdi_transmit("0D0!");
    result = sdi_receive();
    if (result) {
      printf("%.*s\n", sizeof(rxBuffer), rxBuffer);
      if (d0_parser.parseLine(reinterpret_cast<const char *>(rxBuffer), sizeof(rxBuffer))) {
        const Value d0_0 = d0_parser.getValue(1);
        const Value d0_1 = d0_parser.getValue(2);
        const Value d0_2 = d0_parser.getValue(3);
        const Value d0_3 = d0_parser.getValue(4);
        printf("temp_sensor:  %.3f C\n", d0_0.data);
        printf("sp_cond:      %.3f uS/cm\n", d0_1.data);
        printf("pH:           %.3f\n", d0_2.data);
        printf("pH:           %.3f mV\n", d0_3.data);
        _latest_sample.temp_sensor = (float)d0_0.data.double_val;
        _latest_sample.sp_cond = (float)d0_1.data.double_val;
        _latest_sample.pH = (float)d0_2.data.double_val;
        _latest_sample.pH_mV = (float)d0_3.data.double_val;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(100));
    sdi_transmit("0D1!");
    result = sdi_receive();
    if (result) {
      printf("%.*s\n", sizeof(rxBuffer), rxBuffer);
      if (d1_parser.parseLine(reinterpret_cast<const char *>(rxBuffer), sizeof(rxBuffer))) {
        const Value d1_0 = d1_parser.getValue(1);
        const Value d1_1 = d1_parser.getValue(2);
        const Value d1_2 = d1_parser.getValue(3);
        const Value d1_3 = d1_parser.getValue(4);
        printf("DO:           %.3f Percent Sat\n", d1_0.data);
        printf("DO:           %.3f mg/L\n", d1_1.data);
        printf("turbidity:    %.3f NTU\n", d1_2.data);
        printf("wiper pos:    %.3f V\n", d1_3.data);
        _latest_sample.dis_oxy = (float)d1_0.data.double_val;
        _latest_sample.dis_oxy_mg = (float)d1_1.data.double_val;
        _latest_sample.turbidity = (float)d1_2.data.double_val;
        _latest_sample.wiper_pos = (float)d1_3.data.double_val;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(100));
    sdi_transmit("0D2!");
    result = sdi_receive();
    if (result) {
      printf("%.*s\n", sizeof(rxBuffer), rxBuffer);
      if (d2_parser.parseLine(reinterpret_cast<const char *>(rxBuffer), sizeof(rxBuffer))) {
        const Value d2_0 = d2_parser.getValue(1);
        const Value d2_1 = d2_parser.getValue(2);
        printf("depth:        %.3f m\n", d2_0.data);
        printf("power supply: %.3f V\n", d2_1.data);
        _latest_sample.depth = (float)d2_0.data.double_val;
        _latest_sample.power = (float)d2_1.data.double_val;
      }
    }
    break;
  }
}

void SondeEXO3sSensor::inquire_cmd(void) {
  // send 0 - 0! or ?!
  sdi_cmd(SDI_ADDRESS_INQUIRY_CMD);
}

void SondeEXO3sSensor::identify_cmd(void) {
  // send 1 - 0I!
  sdi_cmd(SDI_INDENTIFICATION_CMD);
}

void SondeEXO3sSensor::measure_cmd(void) {
  // send 2 - 0M!
  sdi_cmd(SDI_START_MEASUREMENT_CMD);
}

/**
 * @brief Flushes the data from the sensor driver.
 */
void SondeEXO3sSensor::flush(void) { PLUART::reset(); }
