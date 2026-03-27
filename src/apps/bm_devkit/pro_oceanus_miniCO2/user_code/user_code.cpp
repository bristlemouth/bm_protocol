/*
 *
 * This is a lightweight example Dev Kit application to integrate a
 * miniCO2 sensor with comma-separated data output.
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
#define DEFAULT_BAUD_RATE 19200
#define DEFAULT_LINE_TERM 10 // LF / '\n', 0x0A
#define BYTES_CLUSTER_MS 50

/// For Turning Numbers Into Data
// How often to compute and return statistics
#define CO2_AGG_PERIOD_MIN 5
// 5 min => 300,000 ms
#define CO2_AGG_PERIOD_MS (CO2_AGG_PERIOD_MIN * 60 * 1000)
/* We have enough RAM that we can keep it simple for shorter durations - use 64 bit doubles, buffer all readings.
   We could be much more RAM and precision efficient by using numerical methods like Kahan summation and Welford's algorithm.*/
// 5 minutes @ 0.5Hz + 10 extra samples for padding => 160 samples max
#define MAX_CO2_SAMPLES 160

typedef struct {
  uint16_t sample_count;
  double min;
  double max;
  double mean;
  double stdev;
} __attribute__((__packed__)) co2Data_t;
#define CO2_DATA_SIZE sizeof(co2Data_t)

// Create an instance of the averaging sampler for corrected CO2 data
static AveragingSampler co2_data;

// A timer variable we can set to trigger a pulse on LED2 when we get payload serial data
static int32_t ledLinePulse = -1;
static u_int32_t baud_rate_config = DEFAULT_BAUD_RATE;
static u_int32_t line_term_config = DEFAULT_LINE_TERM;

// A buffer for our data from the payload uart
char payload_buffer[2048];

/// For Turning Text Into Numbers
/*
 * Setup a LineParser to turn the ASCII serial data from the miniCO2 sensor into numbers
 * Lines from the miniCO2 sensor look like:
 *    W M,2026,03,26,22,05,23,01951,02341,445.00,436.29,24.10,1019.97,16.24,12.1
 *    - comma-separated values
 *    - Format: Start marker, Year, Month, Day, Hour, Minute, Second,
 *              Reference A/D, Current A/D, Raw CO2, Corrected CO2, Sensor Temp,
 *              Gas Pressure, IR Detector Temp, Supply Voltage
 */

// Define the value types for all 15 fields
const ValueType valueTypes[] = {
  TYPE_STRING,  // Start marker "W M"
  TYPE_UINT64,  // Year
  TYPE_UINT64,  // Month
  TYPE_UINT64,  // Day
  TYPE_UINT64,  // Hour
  TYPE_UINT64,  // Minute
  TYPE_UINT64,  // Second
  TYPE_UINT64,  // Reference A/D [counts]
  TYPE_UINT64,  // Current A/D [counts]
  TYPE_DOUBLE,  // Raw CO2 [ppm]
  TYPE_DOUBLE,  // Corrected CO2 [ppmv]
  TYPE_DOUBLE,  // Sensor temperature [C]
  TYPE_DOUBLE,  // Gas pressure [millibar]
  TYPE_DOUBLE,  // IR detector internal cell temperature [C]
  TYPE_DOUBLE   // Supply voltage [volts]
};

// Declare the parser with comma separator, buffer length, value types array, and number of values per line.
OrderedSeparatorLineParser parser(",", 512, valueTypes, 15);

void setup(void) {
  /* USER ONE-TIME SETUP CODE GOES HERE */

  // Initialize the CO2 data buffer to the size we need.
  co2_data.initBuffer(MAX_CO2_SAMPLES);
  // Initialize our LineParser, which will allocate any needed memory for parsing.
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
  // This aggregates CO2 readings into stats, and sends them along to Spotter
  static u_int32_t co2StatsTimer = uptimeGetMs();
  if ((u_int32_t)uptimeGetMs() - co2StatsTimer >= CO2_AGG_PERIOD_MS) {
    co2StatsTimer = uptimeGetMs();
    double mean = 0, stdev = 0, min = 0, max = 0;
    uint16_t n_samples = 0;
    if (co2_data.getNumSamples()) {
      mean = co2_data.getMean();
      stdev = co2_data.getStd(mean);
      min = co2_data.getMin();
      max = co2_data.getMax();
      n_samples = co2_data.getNumSamples();
      co2_data.clear();
    }

    // Get the RTC if available
    RTCTimeAndDate_t time_and_date = {};
    rtcGet(&time_and_date);
    char rtcTimeBuffer[32];
    rtcPrint(rtcTimeBuffer, &time_and_date);

    spotter_log(0, "co2_data_agg.log", USE_TIMESTAMP,
               "tick: %llu, rtc: %s, n: %u, min: %.2f, max: %.2f, mean: %.2f, "
               "std: %.2f\n",
               uptimeGetMs(), rtcTimeBuffer, n_samples, min, max, mean, stdev);
    spotter_log_console(0,
              "[co2-agg] | tick: %llu, rtc: %s, n: %u, min: %.2f, max: %.2f, "
              "mean: %.2f, std: %.2f",
              uptimeGetMs(), rtcTimeBuffer, n_samples, min, max, mean, stdev);
    printf("[co2-agg] | tick: %llu, rtc: %s, n: %u, min: %.2f, max: %.2f, "
           "mean: %.2f, std: %.2f\n",
           uptimeGetMs(), rtcTimeBuffer, n_samples, min, max, mean, stdev);
    uint8_t tx_data[CO2_DATA_SIZE] = {};
    co2Data_t tx_co2 = {
        .sample_count = n_samples, .min = min, .max = max, .mean = mean, .stdev = stdev};
    memcpy(tx_data, (uint8_t *)(&tx_co2), CO2_DATA_SIZE);
    if (spotter_tx_data(tx_data, CO2_DATA_SIZE, BmNetworkTypeCellularIriFallback)) {
      printf("%llut - %s | Successfully sent Spotter transmit data request\n", uptimeGetMs(),
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
    spotter_log(0, "miniCO2_raw.log", USE_TIMESTAMP, "tick: %" PRIu64 ", rtc: %s, line: %.*s\n",
               uptimeGetMs(), rtcTimeBuffer, read_len, payload_buffer);
    spotter_log_console(0, "miniCO2 | tick: %" PRIu64 ", rtc: %s, line: %.*s", uptimeGetMs(), rtcTimeBuffer,
              read_len, payload_buffer);
    printf("miniCO2 | tick: %" PRIu64 ", rtc: %s, line: %.*s\n", uptimeGetMs(), rtcTimeBuffer,
           read_len, payload_buffer);

    // trigger a pulse on LED2
    ledLinePulse = uptimeGetMs();

    // Parse the line of text data into numeric values
    if (parser.parseLine(payload_buffer, read_len)) {
      // Extract all parsed values
      const char* start_marker = parser.getValue(0).data.string_val_ptr;
      uint64_t year = parser.getValue(1).data.uint64_val;
      uint64_t month = parser.getValue(2).data.uint64_val;
      uint64_t day = parser.getValue(3).data.uint64_val;
      uint64_t hour = parser.getValue(4).data.uint64_val;
      uint64_t minute = parser.getValue(5).data.uint64_val;
      uint64_t second = parser.getValue(6).data.uint64_val;
      uint64_t ref_ad = parser.getValue(7).data.uint64_val;
      uint64_t curr_ad = parser.getValue(8).data.uint64_val;
      double raw_co2 = parser.getValue(9).data.double_val;
      double corrected_co2 = parser.getValue(10).data.double_val;
      double sensor_temp = parser.getValue(11).data.double_val;
      double gas_pressure = parser.getValue(12).data.double_val;
      double ir_temp = parser.getValue(13).data.double_val;
      double supply_voltage = parser.getValue(14).data.double_val;

      printf("[miniCO2-parsed] | marker: %s, timestamp: %04llu-%02llu-%02llu %02llu:%02llu:%02llu, "
             "ref_ad: %05llu, curr_ad: %05llu, raw_co2: %.2f ppm, corrected_co2: %.2f ppmv, "
             "sensor_temp: %.2f C, pressure: %.2f mbar, ir_temp: %.2f C, voltage: %.1f V\n",
             start_marker, year, month, day, hour, minute, second,
             ref_ad, curr_ad, raw_co2, corrected_co2, sensor_temp, gas_pressure,
             ir_temp, supply_voltage);
    } else {
      printf("Error parsing line!\n");
      return; // Skip this reading if parsing fails
    }

    // Aggregate the corrected CO2 values into statistics
    if (co2_data.getNumSamples() >= MAX_CO2_SAMPLES) {
      printf("ERR - No more room in CO2 reading buffer, already have %d readings!\n",
             MAX_CO2_SAMPLES);
      return;
    }

    double corrected_co2_reading = parser.getValue(10).data.double_val;
    co2_data.addSample(corrected_co2_reading);

    printf("CO2 buffer: count: %lu/%d, min: %.2f ppmv, max: %.2f ppmv\n",
           co2_data.getNumSamples(), MAX_CO2_SAMPLES,
           co2_data.getMin(), co2_data.getMax());
  }
}
