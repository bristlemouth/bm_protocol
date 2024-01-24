#include "user_code.h"
#include "bm_network.h"
#include "bm_printf.h"
#include "bm_pubsub.h"
#include "bsp.h"
#include "debug.h"
#include "lwip/inet.h"
#include "payload_uart.h"
#include "stm32_rtc.h"
#include "task_priorities.h"
#include "uptime.h"
#include "usart.h"
#include "util.h"

#define LED_ON_TIME_MS 20
#define LED_PERIOD_MS 1000
#define DEFAULT_PWM_LED_DUTY (0.0)
#define PWM_LED_CCR1 ((int32_t)(led_pwm_duty * 65535) % 65535)

bool led_state = false;

// A buffer to put data from out payload sensor into.
char payload_buffer[2048];

extern cfg::Configuration* userConfigurationPartition;
float led_pwm_duty = DEFAULT_PWM_LED_DUTY;

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
  IOWrite(&BB_VBUS_EN, 0);
  // ensure Vbus stable before enable Vout with a 5ms delay.
  vTaskDelay(pdMS_TO_TICKS(5));
  // enable Vout, 12V by default.
  IOWrite(&BB_PL_BUCK_EN, 0);
  // Rewrite LED PWM value based on user config.
  userConfigurationPartition->getConfig("ledDutyCycle", strlen("ledDutyCycle"), led_pwm_duty);
  TIM2->CCR1 = PWM_LED_CCR1;
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
    // Toggle the LED each time we get a line from the payload
    if (!led_state) {
      IOWrite(&LED_GREEN, 0);
      led_state = true;
    } else {
      IOWrite(&LED_GREEN, 1);
      led_state = false;
    }

    uint16_t read_len =
        PLUART::readLine(payload_buffer, sizeof(payload_buffer));
    printf("line: %s\n", payload_buffer);

    // Get the RTC if available
    RTCTimeAndDate_t time_and_date = {};
    rtcGet(&time_and_date);
    char rtcTimeBuffer[32];
    rtcPrint(rtcTimeBuffer, &time_and_date);

    // Print the payload data to a file, to the bm_printf console, and to the printf console.
    bm_fprintf(0, "payload_data.log", "tick: %llu, rtc: %s, line: %.*s\n",
               uptimeGetMs(), rtcTimeBuffer, read_len, payload_buffer);
    bm_printf(0, "[payload] | tick: %llu, rtc: %s, line: %.*s", uptimeGetMs(),
              rtcTimeBuffer, read_len, payload_buffer);
    printf("[payload] | tick: %llu, rtc: %s, line: %.*s\n", uptimeGetMs(),
           rtcTimeBuffer, read_len, payload_buffer);
  }
}
