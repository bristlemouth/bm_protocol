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
#define SAMPLE_SIZE sizeof(EXO3sample)
#define MAX_TX_BUFFER_SIZE (uint16_t)(300)

// app_main passes a handle to the user config partition in NVM.
extern cfg::Configuration *userConfigurationPartition;

uint32_t sample_count = DEFAULT_SAMPLE_COUNT;
EXO3sample samples[MAX_SAMPLE_COUNT] = {};
uint32_t current_sample_index = 0;
uint8_t tx_buffer[MAX_TX_BUFFER_SIZE];  // Buffer for data transmission
static SondeEXO3sSensor sondeEXO3sSensor;

// A timer variable we can set to trigger a pulse on LED2 when we get payload serial data
static int32_t ledLinePulse = -1;


void transmit_samples(){
  memset(tx_buffer, 0, sizeof(tx_buffer)); //  Clear the telemetry data buffer.
  size_t tx_len = SAMPLE_SIZE * sample_count;
  memcpy(tx_buffer, &samples, tx_len);

  // Get the RTC if available
  RTCTimeAndDate_t time_and_date = {};
  rtcGet(&time_and_date);
  char rtcTimeBuffer[32];
  rtcPrint(rtcTimeBuffer, &time_and_date);

  // Print hex string of the buffer to the console for debugging.
  static char tx_hex[MAX_TX_BUFFER_SIZE * 2 + 1];
  size_t i;
  for (i = 0; i < tx_len; ++i) {
    sprintf(&tx_hex[i * 2], "%02X", tx_buffer[i]);
  }
  tx_hex[i * 2] = '\0'; // Null-terminate the string

  printf("[exo3-sonde-sdi12] buffer to send | tick: %llu, rtc: %s, buff: %s\n", uptimeGetMs(), rtcTimeBuffer, tx_hex);
  bm_printf(0, "[exo3-sonde-sdi12] buffer to send | tick: %llu, rtc: %s, buff: 0x%s", uptimeGetMs(), rtcTimeBuffer, tx_hex);

  // Transmit over Spotter celluar or Iridium SBD fallback.
  if (spotter_tx_data(tx_buffer, tx_len, BM_NETWORK_TYPE_CELLULAR_IRI_FALLBACK)) {
    printf("%llut - %s | Sucessfully sent Spotter transmit data request\n", uptimeGetMs(), rtcTimeBuffer);
  } else {
    printf("%llut - %s | Failed to send Spotter transmit data request\n", uptimeGetMs(), rtcTimeBuffer);
  }

  // resetting the buffer
  memset(samples, 0, sizeof(samples));
}

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
  printf("Initializing \n");
  sondeEXO3sSensor.init();
  printf("Waking Sensor \n");
  sondeEXO3sSensor.sdi_wake();
  vTaskDelay(pdMS_TO_TICKS(10000));
  sondeEXO3sSensor.inquire_cmd();
  vTaskDelay(pdMS_TO_TICKS(100));
  sondeEXO3sSensor.identify_cmd();
}

void loop(void) {
  /* USER LOOP CODE GOES HERE */
  sondeEXO3sSensor.measure_cmd();
  // accumulate list of these samples
  EXO3sample sens = sondeEXO3sSensor.getLatestSample();
  samples[current_sample_index] = sens;

  // Get the RTC if available
  RTCTimeAndDate_t time_and_date = {};
  rtcGet(&time_and_date);
  char rtcTimeBuffer[32];
  rtcPrint(rtcTimeBuffer, &time_and_date);

  bm_fprintf(0, "exo3s_sonde_raw.log", USE_TIMESTAMP, "tick: %llu, rtc: %s, data --- "
                                       "temp_sensor: %.3f C, "
                                       "sp_cond: %.3f uS/cm, "
                                       "pH: %.3f, "
                                       "pH: %.3f mV, "
                                       "DO: %.3f Percent Sat, "
                                       "DO: %.3f mg/L, "
                                       "turbidity: %.3f NTU, "
                                       "wiper pos: %.3f V, "
                                       "depth: %.3f m, "
                                       "power supply: %.3f V"
                                       "\n", uptimeGetMs(), rtcTimeBuffer, sens.temp_sensor, sens.sp_cond, sens.pH, sens.pH_mV,
             sens.dis_oxy, sens.dis_oxy_mg, sens.turbidity, sens.wiper_pos, sens.depth, sens.power) ;
  bm_printf(0, "[payload] | tick: %llu, rtc: %s, data --- "
           "temp_sensor: %.3f C, "
           "sp_cond: %.3f uS/cm, "
           "pH: %.3f, "
           "pH: %.3f mV, "
           "DO: %.3f Percent Sat, "
           "DO: %.3f mg/L, "
           "turbidity: %.3f NTU, "
           "wiper pos: %.3f V, "
           "depth: %.3f m, "
           "power supply: %.3f V"
           "\n", uptimeGetMs(), rtcTimeBuffer, sens.temp_sensor, sens.sp_cond, sens.pH, sens.pH_mV,
        sens.dis_oxy, sens.dis_oxy_mg, sens.turbidity, sens.wiper_pos, sens.depth, sens.power) ;
  printf("[exo3-sonde-sdi12] | tick: %llu, rtc: %s, data --- "
         "temp_sensor: %.3f C, "
         "sp_cond: %.3f uS/cm, "
         "pH: %.3f, "
         "pH: %.3f mV, "
         "DO: %.3f Percent Sat, "
         "DO: %.3f mg/L, "
         "turbidity: %.3f NTU, "
         "wiper pos: %.3f V, "
         "depth: %.3f m, "
         "power supply: %.3f V"
         "\n", uptimeGetMs(), rtcTimeBuffer, sens.temp_sensor, sens.sp_cond, sens.pH, sens.pH_mV,
         sens.dis_oxy, sens.dis_oxy_mg, sens.turbidity, sens.wiper_pos, sens.depth, sens.power) ;

  current_sample_index++;

  if (current_sample_index == sample_count){
    // TODO: Send data to spotter
    transmit_samples();
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
