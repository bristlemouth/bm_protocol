#include "user_code.h"
#include "spotter.h"
#include "pubsub.h"
#include "bsp.h"
#include "debug.h"
#include "lwip/inet.h"
#include "payload_uart.h"
#include "stm32_rtc.h"
#include "task_priorities.h"
#include "uptime.h"
#include "usart.h"
#include "app_util.h"

#define LED_ON_TIME_MS 20
#define LED_PERIOD_MS 1000

bool led_state = false;

// A buffer to put data from out payload sensor into.
char payload_buffer[2048];

void setup(void) {
  /* USER ONE-TIME SETUP CODE GOES HERE */
  // Setup the UART – the on-board serial driver that talks to the RS232 transceiver.
  PLUART::init(USER_TASK_PRIORITY);
  // Baud set per expected baud rate of the sensor.
  PLUART::setBaud(9600);
  // Set a line termination character per protocol of the sensor.
  PLUART::setTerminationCharacter('\n');
  // Turn on the UART.
  PLUART::enable();
  // Enable the input to the Vout power supply.
  IOWrite(&VBUS_EN, 0);
  // ensure Vbus stable before enable Vout with a 5ms delay.
  vTaskDelay(pdMS_TO_TICKS(5));
  // enable Vout, 12V by default.
  IOWrite(&PL_BUCK_EN, 0);
}

void loop(void) {
  /* USER LOOP CODE GOES HERE */

  // Read a byte if it is available
  while (PLUART::byteAvailable()) {
    uint8_t byte_read = PLUART::readByte();
    printf("byte: %c\n", (char)byte_read);
  }

  // Read a line if it is available
  if (PLUART::lineAvailable()) {
    uint16_t read_len =
        PLUART::readLine(payload_buffer, sizeof(payload_buffer));
    printf("line: %s\n", payload_buffer);

    // Get the RTC if available
    RTCTimeAndDate_t time_and_date = {};
    rtcGet(&time_and_date);
    char rtcTimeBuffer[32];
    rtcPrint(rtcTimeBuffer, &time_and_date);

    // Print the payload data to a file, to the spotter_log_console console, and to the printf console.
    spotter_log(0, "payload_data.log", USE_TIMESTAMP, "tick: %llu, rtc: %s, line: %.*s\n",
               uptimeGetMs(), rtcTimeBuffer, read_len, payload_buffer);
    spotter_log_console(0, "[payload] | tick: %llu, rtc: %s, line: %.*s", uptimeGetMs(),
              rtcTimeBuffer, read_len, payload_buffer);
    printf("[payload] | tick: %llu, rtc: %s, line: %.*s\n", uptimeGetMs(),
           rtcTimeBuffer, read_len, payload_buffer);
  }
}
