#include "debug.h"
#include "util.h"
#include "user_code.h"
#include "bristlefin.h"
#include "bsp.h"
#include "stm32_rtc.h"
#include "task_priorities.h"
#include "uptime.h"
#include "bm_printf.h"
#include "bm_pubsub.h"
#include "lwip/inet.h"
#include "bm_network.h"
#include "usart.h"
#include "payload_uart.h"
#include "LineParser.h"
#include "array_utils.h"

#define LED_ON_TIME_MS 20
#define LED_PERIOD_MS 1000

/// For Turning Numbers Into Data
// How often to compute and return statistics
#define PRESSURE_AGG_PERIOD_MIN 5
// 10 min => 600,000 ms
#define PRESSURE_AGG_PERIOD_MS (PRESSURE_AGG_PERIOD_MIN * 60 * 1000)
/* We have enough RAM that we can keep it simple for shorter durations - use 64 bit doubles, buffer all readings.
   We could be much more RAM and precision efficient by using numerical methods like Kahan summation and Welford's algorithm.*/
#define MAX_PRESSURE_SAMPLES ((PRESSURE_AGG_PERIOD_MS / 500) + 10) // 10 minutes @ 2Hz + 10 extra samples for padding => 1210, ~10k RAM
typedef struct {
  uint16_t sample_count;
  double min;
  double max;
  double mean;
  double stdev;
  double* values;
} __attribute__((__packed__)) pressureData_t;
// We'll allocate the values buffer later.
pressureData_t pressure_data = {};

/// For Turning Text Into Numbers
/*
 * Setup a LineParser to turn the ASCII serial data from the RBR Coda3 into numbers
 * Lines from the RBR Coda3 Depth sensor look like:
 *    80000, 10.1433
 *    - comma-separated values
 *    - an unsigned integer representing the tick time of the Coda sensor
 *    - a floating point number representing the pressure in deci-bar
 *    - a 256 character buffer should be more than enough */
size_t maxLineLen = 256;
const char* separator = ",";
// For unsigned ints, let's use 64 bits, and for floating point let's use 64 bit doubles.
//   We've got luxurious amounts of RAM on this chip, and it's much easier to avoid roll-overs and precision issues
//   by using it vs. troubleshooting them because we prematurely optimized things.
ValueType valueTypes[] = {TYPE_UINT64, TYPE_DOUBLE};
// Declare the parser here with separator, buffer length, value types array, and number of values per line.
//   We'll initialize the parser later in setup to allocate all the memory we'll need.
LineParser parser(",", 256, valueTypes, 2);

// A timer variable we can set to trigger a pulse on LED2 when we get payload serial data
static int32_t ledLinePulse = -1;
/*
 * This function is called from the payload UART library in src/lib/bristlefin/payload_uart.cpp::processLine function.
 * Every time the uart receives the configured termination character (newline character by default),
 * It will:
 * -- print the line to Dev Kit USB console.
 * -- print the line to Spotter USB console.
 * -- write the line to a payload_data.log on the Spotter SD card.
 * -- call this function, so we can do custom things with the data.
 * In this case, we parse and evalute new pressure readings.
 * */
void PLUART::userProcessLine(uint8_t *line, size_t len) {
  /// NOTE - this function is called from the LPUartRx task. Interacting with the same data as setup() and loop(),
  ///   which are called from the USER task, is not thread safe!

  // trigger a pulse on LED2
  ledLinePulse = uptimeGetMs();
  // Now when we get a line of text data, our LineParser turns it into numeric values.
  if (parser.parseLine(reinterpret_cast<const char*>(line), len)) {
    printf("parsed values: %llu | %f\n", parser.getValue(0).data, parser.getValue(1).data);
  }
  else {
    printf("Error parsing line!\n");
    return;
  }
  // Now let's aggregate those values into statistics
  if (pressure_data.sample_count >= MAX_PRESSURE_SAMPLES) {
    printf("ERR - No more room in pressure reading buffer, already have %d readings!\n", MAX_PRESSURE_SAMPLES);
    return;
  }
  if (xUserDataMutex == NULL) {
    printf("ERR - user data Mutex NULL!\n");
    return;
  }
  if(xSemaphoreTake(xUserDataMutex, portMAX_DELAY) == pdTRUE) {
    double pressure_reading = parser.getValue(1).data.double_val;
    pressure_data.values[pressure_data.sample_count++] = pressure_reading;
    if (pressure_data.sample_count == 1) {
      pressure_data.max = pressure_reading;
      pressure_data.min = pressure_reading;
    } else if (pressure_reading < pressure_data.min) pressure_data.min = pressure_reading;
    else if (pressure_reading > pressure_data.max) pressure_data.max = pressure_reading;
    printf("count: %u/%d, min: %f, max: %f\n", pressure_data.sample_count, MAX_PRESSURE_SAMPLES, pressure_data.min, pressure_data.max);
    xSemaphoreGive(xUserDataMutex);
  }
}

void setup(void) {
  /* USER ONE-TIME SETUP CODE GOES HERE */
  // Allocate memory for pressure data buffer.
  pressure_data.values = static_cast<double *>(pvPortMalloc(sizeof(double ) * MAX_PRESSURE_SAMPLES));
  // Initialize our LineParser, which will allocated any needed memory for parsing.
  parser.init();
  // Setup the UART – the on-board serial driver that talks to the RS232 transceiver.
  PLUART::initPayloadUart(USER_TASK_PRIORITY);
  // Baud set per expected baud rate of the sensor.
  PLUART::setBaud(9600);
  // Set a line termination character per protocol of the sensor.
  PLUART::setTerminationCharacter('\n');
  // Turn on the UART.
  serialEnable(&PLUART::uart_handle);
  // Enable the input to the Vout power supply.
  BF::enableVbus();
  // ensure Vbus stable before enable Vout with a 5ms delay.
  vTaskDelay(pdMS_TO_TICKS(5));
  // enable Vout, 12V by default.
  BF::enableVout();
}

void loop(void) {
  /* USER LOOP CODE GOES HERE */
  /// This aggregates pressure readings into stats, and sends them along to Spotter
  static u_int32_t pressureStatsTimer = uptimeGetMs();
  if ((u_int32_t)uptimeGetMs() - pressureStatsTimer >= PRESSURE_AGG_PERIOD_MS) {
    pressureStatsTimer = uptimeGetMs();
    double mean = 0, stdev = 0, min = 0, max = 0;
    uint16_t n_samples = 0;
    if(xSemaphoreTake(xUserDataMutex, portMAX_DELAY) == pdTRUE && pressure_data.sample_count) {
      mean = getMean(pressure_data.values, pressure_data.sample_count);
      stdev = getStd(pressure_data.values, pressure_data.sample_count, mean);
      min = pressure_data.min;
      max = pressure_data.max;
      n_samples = pressure_data.sample_count;
      pressure_data.sample_count = 0;
      xSemaphoreGive(xUserDataMutex);
    }
    RTCTimeAndDate_t timeAndDate;
    char rtcTimeBuffer[32];
    if (rtcGet(&timeAndDate) == pdPASS) {
      sprintf(rtcTimeBuffer, "%04u-%02u-%02uT%02u:%02u:%02u.%03u",
              timeAndDate.year,
              timeAndDate.month,
              timeAndDate.day,
              timeAndDate.hour,
              timeAndDate.minute,
              timeAndDate.second,
              timeAndDate.ms);
    } else {
      strcpy(rtcTimeBuffer, "0");
    }

    bm_fprintf(0, "payload_data_agg.log",
               "tick: %llu, rtc: %s, n: %u, min: %.4f, max: %.4f, mean: %.4f, std: %.4f\n",
               uptimeGetMs(), rtcTimeBuffer, n_samples, min, max, mean, stdev);
    bm_printf(0, "[rbr-agg] | tick: %llu, rtc: %s, n: %u, min: %.4f, max: %.4f, mean: %.4f, std: %.4f",
              uptimeGetMs(), rtcTimeBuffer, n_samples, min, max, mean, stdev);
    printf("[rbr-agg] | tick: %llu, rtc: %s, n: %u, min: %.4f, max: %.4f, mean: %.4f, std: %.4f\n",
           uptimeGetMs(), rtcTimeBuffer, n_samples, min, max, mean, stdev);
    uint8_t tx_data[34] = {};
    pressureData_t tx_pressure = {
        .sample_count = n_samples,
        .min = min,
        .max = max,
        .mean = mean,
        .stdev = stdev,
        .values = NULL
    };
    memcpy(tx_data, (uint8_t*)(&tx_pressure), 34);
    if(bm_network_publish(tx_data, 34, BM_NETWORK_TYPE_CELLULAR_IRI_FALLBACK)){
      printf("%llut - %s | Sucessfully sent Spotter transmit data request\n", uptimeGetMs(), rtcTimeBuffer);
    }
    else {
      printf("%llut - %s | Failed to send Spotter transmit data request\n", uptimeGetMs(), rtcTimeBuffer);
    }
  }

  /// This checks for a trigger set by ledLinePulse when data is received from the payload UART.
  ///   Each time this happens, we pulse LED2 Green.
  static bool led2State = false;
  // If LED2 is off and the ledLinePulse flag is set (done in our payload UART process line function), turn it on Green.
  if (!led2State && ledLinePulse > -1) {
    BF::setLed(2, BF::LED_GREEN);
    led2State = true;
  }
  // If LED2 has been on for LED_ON_TIME_MS, turn it off.
  else if (led2State && ((u_int32_t)uptimeGetMs() - ledLinePulse >= LED_ON_TIME_MS)) {
    BF::setLed(2, BF::LED_OFF);
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
    BF::setLed(1, BF::LED_GREEN);
    ledOnTimer = uptimeGetMs();
    ledPulseTimer += LED_PERIOD_MS;
    led1State = true;
  }
    // If LED1 has been on for LED_ON_TIME_MS milliseconds, turn it off.
  else if (led1State && ((u_int32_t)uptimeGetMs() - ledOnTimer >= LED_ON_TIME_MS)) {
    BF::setLed(1, BF::LED_OFF);
    led1State = false;
  }
  /*
    DO NOT REMOVE
    This vTaskDelay delay is REQUIRED for the FreeRTOS task scheduler
    to allow for lower priority tasks to be serviced.
    Keep this delay in the range of 10 to 100 ms.
  */
  vTaskDelay(pdMS_TO_TICKS(10));
}
