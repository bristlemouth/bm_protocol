/*
 *
 * This is a lightweight example Dev Kit application to integrate an
 * RBRcoda³T temperature sensor or an RBRcoda³P pressure sensor.
 *
 */

#include "user_code.h"
#include "LineParser.h"
#include "OrderedSeparatorLineParser.h"
#include "app_util.h"
#include "array_utils.h"
#include "avgSampler.h"
#include "bristlefin.h"
#include "bsp.h"
#include "debug.h"
#include "lwip/inet.h"
#include "payload_uart.h"
#include "pubsub.h"
#include "sensors.h"
#include "spotter.h"
#include "stm32_rtc.h"
#include "task_priorities.h"
#include "uptime.h"
#include "usart.h"

#define LED_ON_TIME_MS 20
#define LED_PERIOD_MS 1000
#define DEFAULT_BAUD_RATE 9600
#define DEFAULT_LINE_TERM 10 // FL / '\n', 0x0A
#define BYTES_CLUSTER_MS 50

/// For Turning Numbers Into Data
// How often to compute and return statistics
#define CODA_AGG_PERIOD_MIN 5
// 10 min => 600,000 ms
#define CODA_AGG_PERIOD_MS (CODA_AGG_PERIOD_MIN * 60 * 1000)
/* We have enough RAM that we can keep it simple for shorter durations - use 64 bit doubles, buffer all readings.
   We could be much more RAM and precision efficient by using numerical methods like Kahan summation and Welford's algorithm.*/
// 10 minutes @ 2Hz + 10 extra samples for padding => 1210, ~10k RAM
#define MAX_CODA_SAMPLES ((CODA_AGG_PERIOD_MS / 500) + 10)
typedef struct {
  uint16_t sample_count;
  double min;
  double max;
  double mean;
  double stdev;
} __attribute__((__packed__)) codaData_t;
#define CODA_DATA_SIZE sizeof(codaData_t)

// Create an instance of the averaging sampler for our data
static AveragingSampler coda_data;

// A timer variable we can set to trigger a pulse on LED2 when we get payload serial data
static int32_t ledLinePulse = -1;
static u_int32_t baud_rate_config = DEFAULT_BAUD_RATE;
static u_int32_t line_term_config = DEFAULT_LINE_TERM;

// A buffer for our data from the payload uart
char payload_buffer[2048];

/// For Turning Text Into Numbers
/*
 * Setup a LineParser to turn the ASCII serial data from the RBR Coda3 into numbers
 * Lines from the RBR Coda3 Depth sensor look like:
 *    80000, 10.1433
 *    - comma-separated values
 *    - an unsigned integer representing the tick time of the Coda sensor
 *    - a floating point number representing the temperature in ºC for T model, pressure in deci-bar for P model
 *    - a 256 character buffer should be more than enough */
// For unsigned ints, let's use 64 bits, and for floating point let's use 64 bit doubles.
//   We've got luxurious amounts of RAM on this chip, and it's much easier to avoid roll-overs and precision issues
//   by using it vs. troubleshooting them because we prematurely optimized things.
const ValueType valueTypes[] = {TYPE_UINT64, TYPE_DOUBLE};
// Declare the parser here with separator, buffer length, value types array, and number of values per line.
//   We'll initialize the parser later in setup to allocate all the memory we'll need.
OrderedSeparatorLineParser parser(",", 256, valueTypes, 2);

void setup(void) {
  /* USER ONE-TIME SETUP CODE GOES HERE */

  // Initialize the coda data buffer to be the size we need.
  coda_data.initBuffer(MAX_CODA_SAMPLES);
  // Initialize our LineParser, which will allocated any needed memory for parsing.
  parser.init();
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
  bristlefin.enableVbus();
  // ensure Vbus stable before enable Vout with a 5ms delay.
  vTaskDelay(pdMS_TO_TICKS(5));
  // enable Vout, 12V by default.
  bristlefin.enableVout();
}

void loop(void) {
  /* USER LOOP CODE GOES HERE */
  // This aggregates coda readings into stats, and sends them along to Spotter
  static u_int32_t codaStatsTimer = uptimeGetMs();
  if ((u_int32_t)uptimeGetMs() - codaStatsTimer >= CODA_AGG_PERIOD_MS) {
    codaStatsTimer = uptimeGetMs();
    double mean = 0, stdev = 0, min = 0, max = 0;
    uint16_t n_samples = 0;
    if (coda_data.getNumSamples()) {
      mean = coda_data.getMean();
      stdev = coda_data.getStd(mean);
      min = coda_data.getMin();
      max = coda_data.getMax();
      n_samples = coda_data.getNumSamples();
      coda_data.clear();
    }

    // Get the RTC if available
    RTCTimeAndDate_t time_and_date = {};
    rtcGet(&time_and_date);
    char rtcTimeBuffer[32];
    rtcPrint(rtcTimeBuffer, &time_and_date);

    spotter_log(0, "payload_data_agg.log", USE_TIMESTAMP,
               "tick: %llu, rtc: %s, n: %u, min: %.4f, max: %.4f, mean: %.4f, "
               "std: %.4f\n",
               uptimeGetMs(), rtcTimeBuffer, n_samples, min, max, mean, stdev);
    spotter_log_console(0,
              "[rbr-agg] | tick: %llu, rtc: %s, n: %u, min: %.4f, max: %.4f, "
              "mean: %.4f, std: %.4f",
              uptimeGetMs(), rtcTimeBuffer, n_samples, min, max, mean, stdev);
    printf("[rbr-agg] | tick: %llu, rtc: %s, n: %u, min: %.4f, max: %.4f, "
           "mean: %.4f, std: %.4f\n",
           uptimeGetMs(), rtcTimeBuffer, n_samples, min, max, mean, stdev);
    uint8_t tx_data[CODA_DATA_SIZE] = {};
    codaData_t tx_coda = {
        .sample_count = n_samples, .min = min, .max = max, .mean = mean, .stdev = stdev};
    memcpy(tx_data, (uint8_t *)(&tx_coda), CODA_DATA_SIZE);
    if (spotter_tx_data(tx_data, CODA_DATA_SIZE, BmNetworkTypeCellularIriFallback)) {
      printf("%llut - %s | Sucessfully sent Spotter transmit data request\n", uptimeGetMs(),
             rtcTimeBuffer);
    } else {
      printf("%llut - %s | Failed to send Spotter transmit data request\n", uptimeGetMs(),
             rtcTimeBuffer);
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
    printf("[payload-bytes] | tick: %" PRIu64 ", rtc: %s, bytes:", uptimeGetMs(),
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
    char rtcTimeBuffer[32] = {};
    rtcPrint(rtcTimeBuffer, NULL);
    spotter_log(0, "rbr_raw.log", USE_TIMESTAMP, "tick: %" PRIu64 ", rtc: %s, line: %.*s\n",
               uptimeGetMs(), rtcTimeBuffer, read_len, payload_buffer);
    spotter_log_console(0, "rbr | tick: %" PRIu64 ", rtc: %s, line: %.*s", uptimeGetMs(), rtcTimeBuffer,
              read_len, payload_buffer);
    printf("rbr | tick: %" PRIu64 ", rtc: %s, line: %.*s\n", uptimeGetMs(), rtcTimeBuffer,
           read_len, payload_buffer);

    // trigger a pulse on LED2
    ledLinePulse = uptimeGetMs();
    // Now when we get a line of text data, our LineParser turns it into numeric values.
    if (parser.parseLine(payload_buffer, read_len)) {
      printf("parsed values: %llu | %f\n", parser.getValue(0).data, parser.getValue(1).data);
    } else {
      printf("Error parsing line!\n");
      return; // FIXME: this is a little confusing
    }
    // Now let's aggregate those values into statistics
    if (coda_data.getNumSamples() >= MAX_CODA_SAMPLES) {
      printf("ERR - No more room in coda reading buffer, already have %d "
             "readings!\n",
             MAX_CODA_SAMPLES);
      return; // FIXME: this is a little confusing
    }

    double coda_reading = parser.getValue(1).data.double_val;
    coda_data.addSample(coda_reading);

    printf("count: %u/%d, min: %f, max: %f\n", coda_data.getNumSamples(), MAX_CODA_SAMPLES,
           coda_data.getMin(), coda_data.getMax());
  }
}
