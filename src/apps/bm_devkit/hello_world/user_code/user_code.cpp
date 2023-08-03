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
#include "sensors.h"

#define LED_ON_TIME_MS 20
#define LED_PERIOD_MS 1000

void setup(void) {
  /* USER ONE-TIME SETUP CODE GOES HERE */
}

void loop(void) {
  /* USER LOOP CODE GOES HERE */
  /// This section demonstrates a simple non-blocking bare metal method for rollover-safe timed tasks,
  ///   like blinking an LED.
  /// More canonical (but more arcane) modern methods of implementing this kind functionality
  ///   would bee to use FreeRTOS tasks or hardware timer ISRs.
  static u_int32_t ledPulseTimer = uptimeGetMs();
  static u_int32_t ledOnTimer = 0;
  static bool ledState = false;
  // Turn LED1 on green every LED_PERIOD_MS milliseconds.
  if (!ledState && ((u_int32_t)uptimeGetMs() - ledPulseTimer >= LED_PERIOD_MS)) {
    bristlefin.setLed(1, Bristlefin::LED_GREEN);
    ledOnTimer = uptimeGetMs();
    ledPulseTimer += LED_PERIOD_MS;
    ledState = true;
  }
  // If LED1 has been on for LED_ON_TIME_MS milliseconds, turn it off.
  else if (ledState && ((u_int32_t)uptimeGetMs() - ledOnTimer >= LED_ON_TIME_MS)) {
    bristlefin.setLed(1, Bristlefin::LED_OFF);
    ledState = false;
  }
  /*
    DO NOT REMOVE
    This vTaskDelay delay is REQUIRED for the FreeRTOS task scheduler
    to allow for lower priority tasks to be serviced.
    Keep this delay in the range of 10 to 100 ms.
  */
  vTaskDelay(pdMS_TO_TICKS(10));
}
