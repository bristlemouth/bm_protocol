#include "user_code.h"
#include "app_util.h"
#include "bsp.h"
#include "debug.h"
#include "lwip/inet.h"
#include "payload_uart.h"
#include "pubsub.h"
#include "spotter.h"
#include "stm32_rtc.h"
#include "task_priorities.h"
#include "uptime.h"
#include "usart.h"

#define LED_ON_TIME_MS 75
#define LED_PERIOD_MS 5000
#define DEFAULT_BAUD_RATE 9600
#define DEFAULT_LINE_TERM 10 // newline, '\n', 0x0A
#define BYTES_CLUSTER_MS 50  // used for console printing convenience
#define DEFAULT_UART_MODE MODE_RS232

// variables to store configurations retrieved from NVM
static u_int32_t baud_rate_config = DEFAULT_BAUD_RATE;
static u_int32_t line_term_config = DEFAULT_LINE_TERM;
static u_int32_t bm_log_enable = false;

// A buffer to put Rx data from our payload sensor into.
char payload_buffer[2048];

// A timer variable we can set to trigger a pulse on LED2 when we get payload serial data
static int32_t ledLinePulse = -1;

void setup(void) {
  /* USER ONE-TIME SETUP CODE GOES HERE */
  // Retrieve user-set config values out of NVM.
  get_config_uint(BM_CFG_PARTITION_USER, "plUartBaudRate", strlen("plUartBaudRate"),
                  &baud_rate_config);
  get_config_uint(BM_CFG_PARTITION_USER, "plUartLineTerm", strlen("plUartLineTerm"),
                  &line_term_config);
  get_config_uint(BM_CFG_PARTITION_SYSTEM, "sensorBmLogEnable", strlen("sensorBmLogEnable"),
                  &bm_log_enable);

  // Setup the UART – the on-board serial driver that talks to the RS232 transceiver.
  PLUART::init(USER_TASK_PRIORITY);
  // Baud set per expected baud rate of the sensor.
  PLUART::setBaud(baud_rate_config);
  // Enable passing raw bytes to user app.
  PLUART::setUseByteStreamBuffer(true);
  // Enable parsing lines and passing to user app.
  /// Warning: PLUART only stores a single line at a time. If your attached payload sends lines
  /// faster than the app reads them, they will be overwritten and data will be lost.
  PLUART::setUseLineBuffer(true);
  // Set a line termination character per protocol of the sensor.
  PLUART::setTerminationCharacter((char)line_term_config);

  // Turn on the UART.
  PLUART::enable();
  // Enable the input to the Vout power supply.
  IOWrite(&BB_VBUS_EN, 0);
  // ensure Vbus stable before enable Vout with a 5ms delay.
  vTaskDelay(pdMS_TO_TICKS(5));
  // enable Vout, 12V by default.
  IOWrite(&BB_PL_BUCK_EN, 0);

  // turn off LEDs
  IOWrite(&LED_BLUE, 0);
  IOWrite(&LED_GREEN, 0);
  IOWrite(&LED_RED, 0);
}

void loop(void) {
  /* USER LOOP CODE GOES HERE */
  // Read a cluster of Rx bytes if available
  // -- A timer is used to try to keep clusters of bytes (say from lines) in the same output.
  static int64_t readingBytesTimer = -1;
  // Note - PLUART::setUseByteStreamBuffer must be set true in setup to enable bytes.
  if (readingBytesTimer == -1 && PLUART::byteAvailable()) {
    // Get the RTC if available
    RTCTimeAndDate_t time_and_date = {};
    rtcGet(&time_and_date);
    char rtcTimeBuffer[32];
    rtcPrint(rtcTimeBuffer, &time_and_date);
    printf("[payload-bytes] | tick: %llu, rtc: %s, bytes:", uptimeGetMs(), rtcTimeBuffer);
    // not very readable, but it's a compact trick to overload our timer variable with a -1 flag
    readingBytesTimer = (int64_t)((u_int32_t)uptimeGetMs());
  }
  while (PLUART::byteAvailable()) {
    readingBytesTimer = (int64_t)((u_int32_t)uptimeGetMs());
    uint8_t byte_read = PLUART::readByte();
    printf("%02X ", byte_read);
  }
  if (readingBytesTimer > -1 &&
      (u_int32_t)uptimeGetMs() - (u_int32_t)readingBytesTimer >= BYTES_CLUSTER_MS) {
    printf("\n");
    readingBytesTimer = -1;
  }

  // Read a line if it is available
  // Note - PLUART::setUseLineBuffer must be set true in setup to enable lines.
  if (PLUART::lineAvailable()) {
    // Shortcut the raw bytes cluster completion so the parsed line will be on a new console line
    if (readingBytesTimer > -1) {
      printf("\n");
      readingBytesTimer = -1;
    }
    uint16_t read_len = PLUART::readLine(payload_buffer, sizeof(payload_buffer));

    // Get the RTC if available
    RTCTimeAndDate_t time_and_date = {};
    rtcGet(&time_and_date);
    char rtcTimeBuffer[32];
    rtcPrint(rtcTimeBuffer, &time_and_date);

    // Based on configuration, print the payload data to a file, to the spotter_log_console console, and to the printf console.
    if (bm_log_enable) {
      spotter_log(0, "payload_data.log", USE_TIMESTAMP, "tick: %llu, rtc: %s, line: %.*s\n",
                 uptimeGetMs(), rtcTimeBuffer, read_len, payload_buffer);
      spotter_log_console(0, "[payload] | tick: %llu, rtc: %s, line: %.*s", uptimeGetMs(), rtcTimeBuffer,
                read_len, payload_buffer);
    }
    printf("[payload-line] | tick: %llu, rtc: %s, line: %.*s\n", uptimeGetMs(), rtcTimeBuffer,
           read_len, payload_buffer);

    ledLinePulse = uptimeGetMs(); // trigger a pulse on LED
  }

  static bool led2State = false;
  /// Flash the LED blue if we received Rx data
  if (!led2State && ledLinePulse > -1) {
    IOWrite(&LED_BLUE, 1);
    led2State = true;
  }
  // If LED2 has been on for LED_ON_TIME_MS, turn it off.
  else if (led2State && ((u_int32_t)uptimeGetMs() - ledLinePulse >= LED_ON_TIME_MS)) {
    IOWrite(&LED_BLUE, 0);
    ledLinePulse = -1;
    led2State = false;
  }

  /// Blink the LED green periodically for sign of life.
  static u_int32_t ledPulseTimer = uptimeGetMs();
  static u_int32_t ledOnTimer = 0;
  static bool led1State = false;
  // Turn LED1 on green every LED_PERIOD_MS milliseconds.
  if (!led1State && ((u_int32_t)uptimeGetMs() - ledPulseTimer >= LED_PERIOD_MS)) {
    IOWrite(&LED_GREEN, 1);
    ledOnTimer = uptimeGetMs();
    ledPulseTimer += LED_PERIOD_MS;
    led1State = true;
  }
  // If LED1 has been on for LED_ON_TIME_MS milliseconds, turn it off.
  else if (led1State && ((u_int32_t)uptimeGetMs() - ledOnTimer >= LED_ON_TIME_MS)) {
    IOWrite(&LED_GREEN, 0);
    led1State = false;
  }
}
