#include "avgSampler.h"
#include "user_code.h"
#include "LineParser.h"
#include "array_utils.h"
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
#include "payload_uart.h"
#include "OrderedSeparatorLineParser.h"
#include "array_utils.h"
#include "sensors.h"
#include "util.h"

#define LED_ON_TIME_MS 20
#define LED_PERIOD_MS 1000

/// For Turning Numbers Into Data
// How often to compute and return statistics
#define PRESSURE_AGG_PERIOD_MIN 5
// 10 min => 600,000 ms
#define PRESSURE_AGG_PERIOD_MS (PRESSURE_AGG_PERIOD_MIN * 60 * 1000)
/* We have enough RAM that we can keep it simple for shorter durations - use 64 bit doubles, buffer all readings.
   We could be much more RAM and precision efficient by using numerical methods like Kahan summation and Welford's algorithm.*/
// 10 minutes @ 2Hz + 10 extra samples for padding => 1210, ~10k RAM
#define MAX_PRESSURE_SAMPLES ((PRESSURE_AGG_PERIOD_MS / 500) + 10)
typedef struct {
  uint16_t sample_count;
  double min;
  double max;
  double mean;
  double stdev;
} __attribute__((__packed__)) pressureData_t;
#define PRESSURE_DATA_SIZE sizeof(pressureData_t)

// Create an instance of the averaging sampler for our data
static AveragingSampler pressure_data;

// A buffer for our data from the payload uart
char payload_buffer[2048];

/// For Turning Text Into Numbers
/*
 * Setup a LineParser to turn the ASCII serial data from the RBR Coda3 into numbers
 * Lines from the RBR Coda3 Depth sensor look like:
 *    80000, 10.1433
 *    - comma-separated values
 *    - an unsigned integer representing the tick time of the Coda sensor
 *    - a floating point number representing the pressure in deci-bar
 *    - a 256 character buffer should be more than enough */
// For unsigned ints, let's use 64 bits, and for floating point let's use 64 bit doubles.
//   We've got luxurious amounts of RAM on this chip, and it's much easier to avoid roll-overs and precision issues
//   by using it vs. troubleshooting them because we prematurely optimized things.
ValueType valueTypes[] = {TYPE_UINT64, TYPE_DOUBLE};
// Declare the parser here with separator, buffer length, value types array, and number of values per line.
//   We'll initialize the parser later in setup to allocate all the memory we'll need.
OrderedSeparatorLineParser parser(",", 256, valueTypes, 2);

// A timer variable we can set to trigger a pulse on LED2 when we get payload serial data
static int32_t ledLinePulse = -1;

void setup(void) {
  /* USER ONE-TIME SETUP CODE GOES HERE */

  // Initialize the pressure data buffer to be the size we need.
  pressure_data.initBuffer(MAX_PRESSURE_SAMPLES);
  // Initialize our LineParser, which will allocated any needed memory for parsing.
  parser.init();
  // Setup the UART – the on-board serial driver that talks to the RS232 transceiver.
  PLUART::init(USER_TASK_PRIORITY);
  // Baud set per expected baud rate of the sensor.
  PLUART::setBaud(9600);
  // Set a line termination character per protocol of the sensor.
  PLUART::setTerminationCharacter('\n');
  // Turn on the UART.
  PLUART::enable();
  // Enable the input to the Vout power supply.
  bristlefin.enableVbus();
  // ensure Vbus stable before enable Vout with a 5ms delay.
  vTaskDelay(pdMS_TO_TICKS(5));
  // enable Vout, 12V by default.
  bristlefin.enableVout();
}

void loop(void) {
  /* USER LOOP CODE GOES HERE */
  // This aggregates pressure readings into stats, and sends them along to Spotter
  static u_int32_t pressureStatsTimer = uptimeGetMs();
  if ((u_int32_t)uptimeGetMs() - pressureStatsTimer >= PRESSURE_AGG_PERIOD_MS) {
    pressureStatsTimer = uptimeGetMs();
    double mean = 0, stdev = 0, min = 0, max = 0;
    uint16_t n_samples = 0;
    if (pressure_data.getNumSamples()) {
      mean = pressure_data.getMean();
      stdev = pressure_data.getStd(mean);
      min = pressure_data.getMin();
      max = pressure_data.getMax();
      n_samples = pressure_data.getNumSamples();
      pressure_data.clear();
    }

    // Get the RTC if available
    RTCTimeAndDate_t time_and_date = {};
    rtcGet(&time_and_date);
    char rtcTimeBuffer[32];
    rtcPrint(rtcTimeBuffer, &time_and_date);

    bm_fprintf(0, "payload_data_agg.log",
               "tick: %llu, rtc: %s, n: %u, min: %.4f, max: %.4f, mean: %.4f, "
               "std: %.4f\n",
               uptimeGetMs(), rtcTimeBuffer, n_samples, min, max, mean, stdev);
    bm_printf(0,
              "[rbr-agg] | tick: %llu, rtc: %s, n: %u, min: %.4f, max: %.4f, "
              "mean: %.4f, std: %.4f",
              uptimeGetMs(), rtcTimeBuffer, n_samples, min, max, mean, stdev);
    printf("[rbr-agg] | tick: %llu, rtc: %s, n: %u, min: %.4f, max: %.4f, "
           "mean: %.4f, std: %.4f\n",
           uptimeGetMs(), rtcTimeBuffer, n_samples, min, max, mean, stdev);
    uint8_t tx_data[PRESSURE_DATA_SIZE] = {};
    pressureData_t tx_pressure = {.sample_count = n_samples,
                                  .min = min,
                                  .max = max,
                                  .mean = mean,
                                  .stdev = stdev};
    memcpy(tx_data, (uint8_t *)(&tx_pressure), PRESSURE_DATA_SIZE);
    if (spotter_tx_data(tx_data, PRESSURE_DATA_SIZE, BM_NETWORK_TYPE_CELLULAR_IRI_FALLBACK)) {
      printf("%llut - %s | Sucessfully sent Spotter transmit data request\n",
             uptimeGetMs(), rtcTimeBuffer);
    } else {
      printf("%llut - %s | Failed to send Spotter transmit data request\n",
             uptimeGetMs(), rtcTimeBuffer);
    }
  }

  /// This checks for a trigger set by ledLinePulse when data is received from the payload UART.
  ///   Each time this happens, we pulse LED2 Green.
  static bool led2State = false;
  // If LED2 is off and the ledLinePulse flag is set, turn it on Green.
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

  // Read a line if it is available
  if (PLUART::lineAvailable()) {
    uint16_t read_len =
        PLUART::readLine(payload_buffer, sizeof(payload_buffer));

    char rtcTimeBuffer[32] = {};
    rtcPrint(rtcTimeBuffer, NULL);
    bm_fprintf(0, "rbr_raw.log", "tick: %" PRIu64 ", rtc: %s, line: %.*s\n", uptimeGetMs(), rtcTimeBuffer, read_len, payload_buffer);
    bm_printf(0, "[rbr] | tick: %" PRIu64 ", rtc: %s, line: %.*s", uptimeGetMs(), rtcTimeBuffer, read_len, payload_buffer);
    printf("[rbr] | tick: %" PRIu64 ", rtc: %s, line: %.*s\n", uptimeGetMs(), rtcTimeBuffer, read_len, payload_buffer);

    // trigger a pulse on LED2
    ledLinePulse = uptimeGetMs();
    // Now when we get a line of text data, our LineParser turns it into numeric values.
    if (parser.parseLine(payload_buffer, read_len)) {
      printf("parsed values: %llu | %f\n", parser.getValue(0).data,
             parser.getValue(1).data);
    } else {
      printf("Error parsing line!\n");
      return; // FIXME: this is a little confusing
    }
    // Now let's aggregate those values into statistics
    if (pressure_data.getNumSamples() >= MAX_PRESSURE_SAMPLES) {
      printf("ERR - No more room in pressure reading buffer, already have %d "
             "readings!\n",
             MAX_PRESSURE_SAMPLES);
      return; // FIXME: this is a little confusing
    }

    double pressure_reading = parser.getValue(1).data.double_val;
    pressure_data.addSample(pressure_reading);

    printf("count: %u/%d, min: %f, max: %f\n", pressure_data.getNumSamples(),
           MAX_PRESSURE_SAMPLES, pressure_data.getMin(), pressure_data.getMax());
  }
}
