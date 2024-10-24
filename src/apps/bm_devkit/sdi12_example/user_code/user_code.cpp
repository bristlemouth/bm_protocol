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

#define LED_ON_TIME_MS 20
#define LED_PERIOD_MS 1000
#define BYTES_CLUSTER_MS 50

#define DEFAULT_SAMPLE_COUNT 3
#define MAX_SAMPLE_COUNT 7

// app_main passes a handle to the user config partition in NVM.
extern cfg::Configuration *userConfigurationPartition;

uint32_t sample_count = DEFAULT_SAMPLE_COUNT;
EXO3sample samples[MAX_SAMPLE_COUNT] = {};
uint32_t current_sample_index = 0;

// A timer variable we can set to trigger a pulse on LED2 when we get payload serial data
static int32_t ledLinePulse = -1;
//static u_int32_t baud_rate_config = DEFAULT_BAUD_RATE;
// line_term_config is only the received string termination character
//static u_int32_t line_term_config = DEFAULT_LINE_TERM;

// A buffer for our data from the payload uart
char payload_buffer[2048];
static SondeEXO3sSensor sondeEXO3sSensor;
char result;

void setup(void) {
  // samples per send
  userConfigurationPartition->getConfig("sampleCount", strlen("sampleCount"), sample_count);
  // Enable the input to the Vout power supply.
  bristlefin.enableVbus();
  // ensure Vbus stable before enable Vout with a 5ms delay.
  vTaskDelay(pdMS_TO_TICKS(5));
  // enable Vout, 12V by default.
  bristlefin.enableVout();
  bristlefin.enable3V();
  // Initializing
  sondeEXO3sSensor.init();
  sondeEXO3sSensor.sdi_wake(1000);
  sondeEXO3sSensor.inquire_cmd();
  sondeEXO3sSensor.identify_cmd();
  vTaskDelay(pdMS_TO_TICKS(5000));
}

void loop(void) {
  /* USER LOOP CODE GOES HERE */
//  sondeEXO3sSensor.inquire_cmd();
//  vTaskDelay(pdMS_TO_TICKS(2000));
  sondeEXO3sSensor.measure_cmd();
  // accumulate list of these samples
  EXO3sample sens = sondeEXO3sSensor.getLatestSample();
  samples[current_sample_index] = sens;
  current_sample_index++;
  // TODO: Send data to sd card with bm_fprintf
  if (current_sample_index == sample_count){
    // TODO: Send data to spotter
    // TODO: debug prints
    // TODO:clear samples buffer
    current_sample_index = 0;
  }
  vTaskDelay(pdMS_TO_TICKS(2000));


  static bool led2State = false;
  /// This checks for a trigger set by ledLinePulse when data is received from the payload UART.
  ///   Each time this happens, we pulse LED2 Green.
  // If LED2 is off and the ledLinePulse flag is set (done in our payload UART process line function), turn it on Green.
  if (!led2State && ledLinePulse > -1) {
    bristlefin.setLed(2, Bristlefin::LED_GREEN);
    led2State = true;
  }
  // If LED2 has been on for LED_ON_TIME_MS, turn it off.
  else if (led2State &&
           ((u_int32_t)uptimeGetMs() - ledLinePulse >= LED_ON_TIME_MS)) {
    bristlefin.setLed(2, Bristlefin::LED_OFF);
    ledLinePulse = -1;
    led2State = false;
  }

  //printf("Testing\n");

  /// This section demonstrates a simple non-blocking bare metal method for rollover-safe timed tasks,
  ///   like blinking an LED.
  /// More canonical (but more arcane) modern methods of implementing this kind functionality
  ///   would bee to use FreeRTOS tasks or hardware timer ISRs.
  static u_int32_t ledPulseTimer = uptimeGetMs();
  static u_int32_t ledOnTimer = 0;
  static bool led1State = false;
  // Turn LED1 on green every LED_PERIOD_MS milliseconds.
  if (!led1State &&
      ((u_int32_t)uptimeGetMs() - ledPulseTimer >= LED_PERIOD_MS)) {
    bristlefin.setLed(1, Bristlefin::LED_GREEN);
    ledOnTimer = uptimeGetMs();
    ledPulseTimer += LED_PERIOD_MS;
    led1State = true;
  }
  // If LED1 has been on for LED_ON_TIME_MS milliseconds, turn it off.
  else if (led1State &&
           ((u_int32_t)uptimeGetMs() - ledOnTimer >= LED_ON_TIME_MS)) {
    bristlefin.setLed(1, Bristlefin::LED_OFF);
    led1State = false;
  }

  // Read a cluster of bytes if available
  // -- A timer is used to try to keep clusters of bytes (say from lines) in the same output.
  static int64_t readingBytesTimer = -1;
  // Note - PLUART::setUseByteStreamBuffer must be set true in setup to enable bytes.
  if (readingBytesTimer == -1 && PLUART::byteAvailable()) {
    // Get the RTC if available
    RTCTimeAndDate_t time_and_date = {};
    rtcGet(&time_and_date);
    char rtcTimeBuffer[32];
    rtcPrint(rtcTimeBuffer, &time_and_date);
    printf("[payload-bytes] | tick: %llu, rtc: %s, bytes:", uptimeGetMs(),
           rtcTimeBuffer);
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
//  if (PLUART::lineAvailable()) {
//    // Shortcut the raw bytes cluster completion so the parsed line will be on a new console line
//    if (readingBytesTimer > -1) {
//      printf("\n");
//      readingBytesTimer = -1;
//    }
//    uint16_t read_len =
//        PLUART::readLine(payload_buffer, sizeof(payload_buffer));
//
//    // Get the RTC if available
//    RTCTimeAndDate_t time_and_date = {};
//    rtcGet(&time_and_date);
//    char rtcTimeBuffer[32];
//    rtcPrint(rtcTimeBuffer, &time_and_date);
//
//    // Print the payload data to a file, to the bm_printf console, and to the printf console.
//    bm_fprintf(0, "payload_data.log", "tick: %llu, rtc: %s, line: %.*s\n",
//               uptimeGetMs(), rtcTimeBuffer, read_len, payload_buffer);
//    bm_printf(0, "[payload] | tick: %llu, rtc: %s, line: %.*s", uptimeGetMs(),
//              rtcTimeBuffer, read_len, payload_buffer);
//    printf("[payload-line] | tick: %llu, rtc: %s, line: %.*s\n", uptimeGetMs(),
//           rtcTimeBuffer, read_len, payload_buffer);
//
//    ledLinePulse = uptimeGetMs(); // trigger a pulse on LED2
//  }
}
