#include "debug.h"
#include "util.h"
#include "user_code.h"
#include "bsp.h"
#include "stm32_rtc.h"
#include "task_priorities.h"
#include "uptime.h"
#include "bm_printf.h"
#include "bm_pubsub.h"
#include "lwip/inet.h"
#include "bm_network.h"
#include "usart.h"
#include "payload_uart.h"

#define LED_ON_TIME_MS 20
#define LED_PERIOD_MS 1000

bool led_state = false;

// This function is called from the payload UART library in src/lib/common/payload_uart.cpp::processLine function.
//  Every time the uart receives the configured termination character ('\0' character by default),
//  It will:
//   -- print the line to Dev Kit USB console.
//   -- print the line to Spotter USB console.
//   -- write the line to a payload_data.log on the Spotter SD card.
//   -- call this function, so we can do custom things with the data.
//      In this case, we just set a trigger to pulse LED2 on the Dev Kit.
void PLUART::userProcessLine(uint8_t *line, size_t len) {
  // NOTE - this function is called from the LPUartRx task. Interacting with the same data as setup() and loop(),
  //   which are called from the USER task, is not thread safe!
  ( void )line;
  ( void )len;
}

void setup(void) {
  /* USER ONE-TIME SETUP CODE GOES HERE */
  // Setup the UART – the on-board serial driver that talks to the RS232 transceiver.
  PLUART::initPayloadUart(USER_TASK_PRIORITY);
  // Baud set per expected baud rate of the sensor.
  PLUART::setBaud(9600);
  // Set a line termination character per protocol of the sensor.
  PLUART::setTerminationCharacter('\n');
  // Turn on the UART.
  serialEnable(&PLUART::uart_handle);
  // Enable the input to the Vout power supply.
  IOWrite(&BB_VBUS_EN, 0);
  // ensure Vbus stable before enable Vout with a 5ms delay.
  vTaskDelay(pdMS_TO_TICKS(5));
  // enable Vout, 12V by default.
  IOWrite(&BB_PL_BUCK_EN, 0);
}

void loop(void) {
  /* USER LOOP CODE GOES HERE */

  if(!led_state) {
    IOWrite(&LED_GREEN, 0);
    led_state = true;
  } else {
    IOWrite(&LED_GREEN, 1);
    led_state = false;
  }

  /*
    DO NOT REMOVE
    This vTaskDelay delay is REQUIRED for the FreeRTOS task scheduler
    to allow for lower priority tasks to be serviced.
    Keep this delay in the range of 10 to 100 ms.
  */
  vTaskDelay(pdMS_TO_TICKS(10));
}
