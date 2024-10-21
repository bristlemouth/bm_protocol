#include "user_code.h"
#include "bm_network.h"
#include "bm_printf.h"
#include "pubsub.h"
#include "bsp.h"
#include "debug.h"
#include "loadCellSampler.h"
#include "lwip/inet.h"
#include "nau7802.h"
#include "payload_uart.h"
#include "sensors.h"
#include "stm32_rtc.h"
#include "task_priorities.h"
#include "uptime.h"
#include "usart.h"
#include "app_util.h"

#define LED_ON_TIME_MS 20
#define LED_PERIOD_MS 1000

NAU7802 loadCell(&i2c1, NAU7802_ADDR);

// A timer variable we can set to trigger a pulse on LED2 when we get payload serial data
static int32_t ledLinePulse = -1;
// This function is called from the payload UART library in src/lib/common/payload_uart.cpp::processLine function.
//  Every time the uart receives the configured termination character ('\0' character by default),
//  It will:
//   -- print the line to Dev Kit USB console.
//   -- print the line to Spotter USB console.
//   -- write the line to a payload_data.log on the Spotter SD card.
//   -- call this function, so we can do custom things with the data.
//      In this case, we just set a trigger to pulse LED2 on the Dev Kit.

void setup(void) {
  // Adds the load cell as a sensor for periodic sampling.
  loadCellSamplerInit(&loadCell);
  // Setup the UART – the on-board serial driver that talks to the RS232 transceiver.
  PLUART::init(USER_TASK_PRIORITY);
  // Baud set per expected baud rate of the sensor.
  PLUART::setBaud(9600);
  // Set a line termination character per protocol of the sensor.
  PLUART::setTerminationCharacter('\n');
  // Turn on the UART.
  PLUART::enable();
  // Enable the input to the Vout power supply.
  IOWrite(&BB_VBUS_EN, 0);
  // ensure Vbus stable before enable Vout with a 5ms delay.
  vTaskDelay(pdMS_TO_TICKS(5));
  // enable Vout, 12V by default.
  IOWrite(&BB_PL_BUCK_EN, 0);
}

void loop(void) {
  static bool led2State = false;
  /// This checks for a trigger set by ledLinePulse when data is received from the payload UART.
  ///   Each time this happens, we pulse LED2 Green.
  // If LED2 is off and the ledLinePulse flag is set (done in our payload UART process line function), turn it on Green.
  if (!led2State && ledLinePulse > -1) {
    IOWrite(&LED_GREEN, BB_LED_ON);
    led2State = true;
  }
  // If LED2 has been on for LED_ON_TIME_MS, turn it off.
  else if (led2State && ((u_int32_t)uptimeGetMs() - ledLinePulse >= LED_ON_TIME_MS)) {
    IOWrite(&LED_GREEN, BB_LED_OFF);
    ledLinePulse = -1;
    led2State = false;
  }

  static u_int32_t ledPulseTimer = uptimeGetMs();
  static u_int32_t ledOnTimer = 0;
  static bool led1State = false;
  // Turn on the red LED every LED_PERIOD_MS milliseconds.
  if (!led1State && ((u_int32_t)uptimeGetMs() - ledPulseTimer >= LED_PERIOD_MS)) {
    IOWrite(&LED_RED, BB_LED_ON);
    ledOnTimer = uptimeGetMs();
    ledPulseTimer += LED_PERIOD_MS;
    led1State = true;
  }
  // If the red LED has been on for LED_ON_TIME_MS milliseconds, turn it off.
  else if (led1State && ((u_int32_t)uptimeGetMs() - ledOnTimer >= LED_ON_TIME_MS)) {
    IOWrite(&LED_RED, BB_LED_OFF);
    led1State = false;
  }
}
