#include "debug.h"
#include "util.h"
#include "user_code.h"
#include "bristlefin.h"
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

// A timer variable we can set to trigger a pulse on LED2 when we get payload serial data
static int32_t ledLinePulse = -1;
// This function is called from the payload UART library in src/lib/bristlefin/payload_uart.cpp::processLine function.
//  Every time the uart receives the configured termination character (newline character by default),
//  It will:
//   -- print the line to Dev Kit USB console.
//   -- print the line to Spotter USB console.
//   -- write the line to a payload_data.log on the Spotter SD card.
//   -- call this function, so we can do custom things with the data.
//      In this case, we just set a trigger to pulse LED2 on the Dev Kit.
void PLUART::userProcessLine(uint8_t *line, size_t len) {
  /// NOTE - this function is called from the LPUartRx task. Interacting with the same data as setup() and loop(),
  ///   which are called from the USER task, is not thread safe!
  (void) len; // mark unused, we setup our compiler to treat all Warnings as Errors!
  (void) line; // mark unused, we setup our compiler to treat all Warnings as Errors!
  ledLinePulse = uptimeGetMs(); // trigger a pulse on LED2
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
  BF::enableVbus();
  // ensure Vbus stable before enable Vout with a 5ms delay.
  vTaskDelay(pdMS_TO_TICKS(5));
  // enable Vout, 12V by default.
  BF::enableVout();
}

void loop(void) {
  /* USER LOOP CODE GOES HERE */
  static bool led2State = false;
  /// This checks for a trigger set by ledLinePulse when data is received from the payload UART.
  ///   Each time this happens, we pulse LED2 Green.
  // If LED2 is off and the ledLinePulse flag is set (done in our payload UART process line function), turn it on Green.
  if (!led2State && ledLinePulse > -1) {
    BF::setLed(2, BF::LED_GREEN);
    led2State = true;
  }
  // If LED2 has been on for LED_ON_TIME_MS, turn it off.
  else if (led2State && ((u_int32_t)uptimeGetMs() - ledLinePulse >= LED_ON_TIME_MS)) {
    BF::setLed(2, BF::LED_OFF);
    ledLinePulse = -1;
    led2State = false;
  }

  /// This section demonstrates a simple non-blocking bare metal method for rollover-safe timed tasks,
  ///   like blinking an LED.
  /// More canonical (but more arcane) modern methods of implementing this kind functionality
  ///   would bee to use FreeRTOS tasks or hardware timer ISRs.
  static u_int32_t ledPulseTimer = uptimeGetMs();
  static u_int32_t ledOnTimer = 0;
  static bool led1State = false;
  // Turn LED1 on green every LED_PERIOD_MS milliseconds.
  if (!led1State && ((u_int32_t)uptimeGetMs() - ledPulseTimer >= LED_PERIOD_MS)) {
    BF::setLed(1, BF::LED_GREEN);
    ledOnTimer = uptimeGetMs();
    ledPulseTimer += LED_PERIOD_MS;
    led1State = true;
  }
    // If LED1 has been on for LED_ON_TIME_MS milliseconds, turn it off.
  else if (led1State && ((u_int32_t)uptimeGetMs() - ledOnTimer >= LED_ON_TIME_MS)) {
    BF::setLed(1, BF::LED_OFF);
    led1State = false;
  }
  /*
    DO NOT REMOVE
    This vTaskDelay delay is REQUIRED for the FreeRTOS task scheduler
    to allow for lower priority tasks to be serviced.
    Keep this delay in the range of 10 to 100 ms.
  */
  vTaskDelay(pdMS_TO_TICKS(10));
}
