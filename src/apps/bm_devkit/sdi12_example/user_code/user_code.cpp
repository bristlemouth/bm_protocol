#include "user_code.h"
#include "bm_printf.h"
#include "bristlefin.h"
#include "bsp.h"
#include "debug.h"
#include "exo3s_sensor.h"
#include "lwip/inet.h"
#include "payload_uart.h"
#include "sensors.h"
#include "spotter.h"
#include "stm32_rtc.h"
#include "task_priorities.h"
#include "uptime.h"
#include "usart.h"
#include "util.h"

#define LED_ON_TIME_MS 20
#define LED_PERIOD_MS 1000
#define BYTES_CLUSTER_MS 50

#define DEFAULT_SAMPLE_COUNT 3
#define MAX_SAMPLE_COUNT 7
#define SAMPLE_SIZE sizeof(EXO3sample)
#define MAX_TX_BUFFER_SIZE (uint16_t)(300)

// app_main passes a handle to the user config partition in NVM.

uint32_t sample_count = DEFAULT_SAMPLE_COUNT;
EXO3sample samples[MAX_SAMPLE_COUNT] = {};
uint32_t current_sample_index = 0;
uint8_t tx_buffer[MAX_TX_BUFFER_SIZE]; // Buffer for data transmission
static SondeEXO3sSensor sondeEXO3sSensor;

// A timer variable we can set to trigger a pulse on LED2 when we get payload serial data
static int32_t ledLinePulse = -1;
static void transmit_samples(void);

void transmit_samples() {
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

  // print to spotter console
  spotter_log_console(0, "[exo3-sonde-sdi12] buffer to send | tick: %llu, rtc: %s, buff: 0x%s",
                      uptimeGetMs(), rtcTimeBuffer, tx_hex);

  // Transmit over Spotter celluar or Iridium SBD fallback.
  if (spotter_tx_data(tx_buffer, tx_len, BmNetworkTypeCellularIriFallback)) {
    printf("%llut - %s | Sucessfully sent Spotter transmit data request\n", uptimeGetMs(),
           rtcTimeBuffer);
  } else {
    printf("%llut - %s | Failed to send Spotter transmit data request\n", uptimeGetMs(),
           rtcTimeBuffer);
  }

  // resetting the buffer
  memset(samples, 0, sizeof(samples));
}

void setup(void) {
  // samples per send
  get_config_uint(BM_CFG_PARTITION_USER, "sampleCount", strlen("sampleCount"), &sample_count);
  if (sample_count * SAMPLE_SIZE > MAX_TX_BUFFER_SIZE) {
    printf("sampleCount value is larger than max buffer size: %d, limiting sampleCount to: %d",
           MAX_TX_BUFFER_SIZE, MAX_TX_BUFFER_SIZE / SAMPLE_SIZE);
    sample_count = MAX_TX_BUFFER_SIZE / SAMPLE_SIZE;
  }

  // Enable the input to the Vout power supply.
  bristlefin.enableVbus();
  // ensure Vbus stable before enable Vout with a 5ms delay.
  vTaskDelay(pdMS_TO_TICKS(5));
  // enable Vout, 5V, 12V by default.
  bristlefin.enableVout();
  bristlefin.enable5V();
  bristlefin.enable3V();
  // Initializing
  sondeEXO3sSensor.init();
  // Waking up sensor
  sondeEXO3sSensor.sdi_wake();
  vTaskDelay(pdMS_TO_TICKS(10000));
  // Inquire Sensor address
  sondeEXO3sSensor.inquire_cmd();
  vTaskDelay(pdMS_TO_TICKS(100));
  // Inquire Identification
  sondeEXO3sSensor.identify_cmd();
}

void loop(void) {
  // get sensor readings
  sondeEXO3sSensor.measure_cmd();
  // accumulate list of these samples
  EXO3sample sens = sondeEXO3sSensor.getLatestSample();
  samples[current_sample_index] = sens;

  // Get the RTC if available
  RTCTimeAndDate_t time_and_date = {};
  rtcGet(&time_and_date);
  char rtcTimeBuffer[32];
  rtcPrint(rtcTimeBuffer, &time_and_date);

  // log values in the sd card
  spotter_log(0, "exo3s_sonde_raw.log", USE_TIMESTAMP,
              "tick: %llu, rtc: %s, data --- "
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
              "\n",
              uptimeGetMs(), rtcTimeBuffer, sens.temp_sensor, sens.sp_cond, sens.pH, sens.pH_mV,
              sens.dis_oxy, sens.dis_oxy_mg, sens.turbidity, sens.wiper_pos, sens.depth,
              sens.power);
  // print to spotter console
  spotter_log_console(0,
                      "[exo3-sonde-sdi12] | tick: %llu, rtc: %s, data --- "
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
                      "\n",
                      uptimeGetMs(), rtcTimeBuffer, sens.temp_sensor, sens.sp_cond, sens.pH,
                      sens.pH_mV, sens.dis_oxy, sens.dis_oxy_mg, sens.turbidity, sens.wiper_pos,
                      sens.depth, sens.power);
  // print to console,
  // max console output size is 128, therefore will split the below into 2 printf statements
  printf("[exo3-sonde-sdi12] | tick: %llu, rtc: %s, data --- "
         "temp_sensor: %.3f C, "
         "sp_cond: %.3f uS/cm, "
         "pH: %.3f, \n",
         uptimeGetMs(), rtcTimeBuffer, sens.temp_sensor, sens.sp_cond, sens.pH);
  printf("pH: %.3f mV, "
         "DO: %.3f Percent Sat, "
         "DO: %.3f mg/L, "
         "turbidity: %.3f NTU, "
         "wiper pos: %.3f V, "
         "depth: %.3f m, "
         "power supply: %.3f V\n",
         sens.pH_mV, sens.dis_oxy, sens.dis_oxy_mg, sens.turbidity, sens.wiper_pos, sens.depth,
         sens.power);

  current_sample_index++;
  if (current_sample_index == sample_count) {
    transmit_samples();
    current_sample_index = 0;
  }

  static bool led2State = false;
  /// This checks for a trigger set by ledLinePulse when data is received from the payload UART.
  ///   Each time this happens, we pulse LED2 Green.
  // If LED2 is off and the ledLinePulse flag is set (done in our payload UART process line function), turn it on Green.
  if (!led2State && ledLinePulse > -1) {
    bristlefin.setLed(2, Bristlefin::LED_GREEN);
    led2State = true;
  }
  // If LED2 has been on for LED_ON_TIME_MS, turn it off.
  else if (led2State && ((u_int32_t)uptimeGetMs() - ledLinePulse >= LED_ON_TIME_MS)) {
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
  if (!led1State && ((u_int32_t)uptimeGetMs() - ledPulseTimer >= LED_PERIOD_MS)) {
    bristlefin.setLed(1, Bristlefin::LED_GREEN);
    ledOnTimer = uptimeGetMs();
    ledPulseTimer += LED_PERIOD_MS;
    led1State = true;
  }
  // If LED1 has been on for LED_ON_TIME_MS milliseconds, turn it off.
  else if (led1State && ((u_int32_t)uptimeGetMs() - ledOnTimer >= LED_ON_TIME_MS)) {
    bristlefin.setLed(1, Bristlefin::LED_OFF);
    led1State = false;
  }
}
