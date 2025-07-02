#include "user_code.h"
#include "app_util.h"
#include "bristlefin.h"
#include "bsp.h"
#include "debug.h"
#include "lwip/inet.h"
#include "pubsub.h"
#include "sensors.h"
#include "spotter.h"
#include "stm32_rtc.h"
#include "task_priorities.h"
#include "uptime.h"

#define LED_ON_TIME_MS 20
#define LED_PERIOD_MS 1000

void setup(void) {
  /* USER ONE-TIME SETUP CODE GOES HERE */
}

void loop(void) {
  /* USER LOOP CODE GOES HERE */
  static bool ledState = false;
  static uint64_t ledLastScheduledOnTime = uptimeGetMs();
  const uint64_t elapsedSinceOnTime = uptimeGetMs() - ledLastScheduledOnTime;

  // Turn LED1 on green every LED_PERIOD_MS milliseconds.
  if (!ledState && elapsedSinceOnTime >= LED_PERIOD_MS) {
    ledLastScheduledOnTime += LED_PERIOD_MS;
    bristlefin.setLed(1, Bristlefin::LED_GREEN);
    ledState = true;
  }
  // If LED1 has been on for LED_ON_TIME_MS milliseconds, turn it off.
  else if (ledState && elapsedSinceOnTime >= LED_ON_TIME_MS) {
    bristlefin.setLed(1, Bristlefin::LED_OFF);
    ledState = false;
  }
}
