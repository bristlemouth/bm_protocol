/*
 * This is a demonstration application for the Bristlemouth Development Kit.
 *  - by evanShap July 2023
 * This application:
 * - polls on-board sensors on the Bristlemouth Dev Board.
 * - periodically aggregates this data into statistics.
 * - sends this data to Spotter USB console, SD card, and remote data transmission.
 * - for a detailed guide of developing this application. See the "Developing a Simple Application" guide
 *    here: https://bristlemouth.notion.site/Developer-Kit-User-Guides-e9ca1b3c5a1c41c890d0105f2eb7c4b8
 */

#include "user_code.h"
#include "app_util.h"
#include "array_utils.h"
#include "avgSampler.h"
#include "bristlefin.h"
#include "bsp.h"
#include "debug.h"
#include "htuSampler.h"
#include "lwip/inet.h"
#include "powerSampler.h"
#include "pressureSampler.h"
#include "pubsub.h"
#include "sensorSampler.h"
#include "sensors.h"
#include "spotter.h"
#include "stm32_rtc.h"
#include "task_priorities.h"
#include "uptime.h"

#define LED_ON_TIME_MS 20
#define LED_PERIOD_MS 1000

/// For Turning Numbers Into Data
// How often to compute and return statistics
// example CLI: cfg usr set sensorAggPeriodMin float 1.0
//              cfg usr save
//        `     cfg usr listkeys
//              cfg usr get sensorAggPeriodMin float
#define DEFAULT_SENSOR_AGG_PERIOD_MIN (10.0)
float sensor_agg_period_min = DEFAULT_SENSOR_AGG_PERIOD_MIN;
// eg 10 min => 600,000 ms
#define SENSOR_AGG_PERIOD_MS ((double)sensor_agg_period_min * 60.0 * 1000.0)
/* We have enough RAM that we can keep it simple for shorter durations - use 64 bit doubles, buffer all readings.
   We could be much more RAM and precision efficient by using numerical methods like Kahan summation and Welford's algorithm.*/
// 10 minutes @1Hz + 10 extra samples for padding => 610, ~5k RAM per sensor
#define MAX_SENSOR_SAMPLES                                                                     \
  (((uint64_t)SENSOR_AGG_PERIOD_MS / sys_cfg_sensorsPollIntervalMs) + 10)

// Structures to store aggregated stats for our on-board sensors.
typedef struct {
  uint16_t sample_count;
  double min;
  double max;
  double mean;
  double stdev;
} __attribute__((__packed__)) sensorStatData_t;

static AveragingSampler power_voltage_stats;
static AveragingSampler power_current_stats;
static AveragingSampler hum_stats;
static AveragingSampler temp_stats;
static AveragingSampler pressure_stats;

char stats_print_buffer[750]; // Buffer to store debug print data for the stats aggregation.
// The size of this buffer is trial and error, but 750 should be plenty of space.
RTCTimeAndDate_t statsStartRtc = {}; // Timestamp that tracks the start of aggregation periods.

sensorStatData_t aggregateStats(AveragingSampler &sampler) {
  sensorStatData_t ret_stats = {};
  if (sampler.getNumSamples() > 0) {
    ret_stats.mean = sampler.getMean();
    ret_stats.stdev = sampler.getStd(ret_stats.mean);
    ret_stats.min = sampler.getMin();
    ret_stats.max = sampler.getMax();
    ret_stats.sample_count = sampler.getNumSamples();
    /// NOTE - Reset the buffer.
    sampler.clear();
  }
  return ret_stats;
}

void setup() {
  /* USER ONE-TIME SETUP CODE GOES HERE */
  // Allocate memory for stats data buffers.
  power_voltage_stats.initBuffer(MAX_SENSOR_SAMPLES);
  power_current_stats.initBuffer(MAX_SENSOR_SAMPLES);
  temp_stats.initBuffer(MAX_SENSOR_SAMPLES);
  hum_stats.initBuffer(MAX_SENSOR_SAMPLES);
  pressure_stats.initBuffer(MAX_SENSOR_SAMPLES);
  rtcGet(&statsStartRtc);
  get_config_float(BM_CFG_PARTITION_USER, "sensorAggPeriodMin", strlen("sensorAggPeriodMin"),
                   &sensor_agg_period_min);
}

void loop(void) {
  /* USER LOOP CODE GOES HERE */
  /// This aggregates BMDK sensor readings into stats, and sends them along to Spotter
  static uint32_t sensorStatsTimer = uptimeGetMs();
  static uint32_t statsStartTick = uptimeGetMs();
  if ((uint32_t)uptimeGetMs() - sensorStatsTimer >= SENSOR_AGG_PERIOD_MS) {
    sensorStatsTimer = uptimeGetMs();

    sensorStatData_t temp_tx_data, hum_tx_data, voltage_tx_data, current_tx_data,
        pressure_tx_data;

    temp_tx_data = aggregateStats(temp_stats);
    hum_tx_data = aggregateStats(hum_stats);
    voltage_tx_data = aggregateStats(power_voltage_stats);
    current_tx_data = aggregateStats(power_current_stats);
    pressure_tx_data = aggregateStats(pressure_stats);

    char rtcTimeBuffer[32] = {};
    rtcPrint(rtcTimeBuffer, &statsStartRtc);
    uint32_t statsEndTick = uptimeGetMs();
    sprintf(stats_print_buffer,
            "rtc_start: %s, tick_start: %u, tick_end: %u, "
            "temp_n: %u, temp_min: %.4f, temp_max: %.4f, temp_mean: %.4f, "
            "temp_std: %.4f, "
            "hum_n: %u, hum_min: %.4f, hum_max: %.4f, hum_mean: %.4f, hum_std: "
            "%.4f, "
            "voltage_n: %u, voltage_min: %.4f, voltage_max: %.4f, "
            "voltage_mean: %.4f, voltage_std: %.4f, "
            "current_n: %u, current_min: %.4f, current_max: %.4f, "
            "current_mean: %.4f, current_std: %.4f, "
            "pressure_n: %u, pressure_min: %.4f, pressure_max: %.4f, "
            "pressure_mean: %.4f, pressure_std: %.4f",
            rtcTimeBuffer, statsStartTick, statsEndTick, temp_tx_data.sample_count,
            temp_tx_data.min, temp_tx_data.max, temp_tx_data.mean, temp_tx_data.stdev,
            hum_tx_data.sample_count, hum_tx_data.min, hum_tx_data.max, hum_tx_data.mean,
            hum_tx_data.stdev, voltage_tx_data.sample_count, voltage_tx_data.min,
            voltage_tx_data.max, voltage_tx_data.mean, voltage_tx_data.stdev,
            current_tx_data.sample_count, current_tx_data.min, current_tx_data.max,
            current_tx_data.mean, current_tx_data.stdev, pressure_tx_data.sample_count,
            pressure_tx_data.min, pressure_tx_data.max, pressure_tx_data.mean,
            pressure_tx_data.stdev);
    spotter_log(0, "bmdk_sensor_agg.log", USE_TIMESTAMP, "%s\n", stats_print_buffer);
    spotter_log_console(0, "[sensor-agg] | %s", stats_print_buffer);
    printf("[sensor-agg] | %s\n", stats_print_buffer);
    // Update variables tracking start time of agg period in ticks and RTC.
    statsStartTick = statsEndTick;
    RTCTimeAndDate_t time_and_date = {};
    rtcGet(&time_and_date);
    statsStartRtc = time_and_date;
    // Setup raw bytes for Tx data buffer.
    static const uint8_t N_STAT_ELEM_BYTES = sizeof(sensorStatData_t);
    sensorStatData_t *tx_data_structs[] = {&temp_tx_data, &hum_tx_data, &voltage_tx_data,
                                           &current_tx_data, &pressure_tx_data};
    static const uint8_t N_STAT_SENSORS = sizeof(tx_data_structs) / sizeof(tx_data_structs[0]);
    static const uint16_t N_TX_DATA_BYTES = N_STAT_ELEM_BYTES * N_STAT_SENSORS;
    uint8_t tx_data[N_TX_DATA_BYTES] = {};
    // Iterate through all of our aggregated stats, and copy data to our tx buffer.
    for (uint8_t i = 0; i < N_STAT_SENSORS; i++) {
      memcpy(tx_data + N_STAT_ELEM_BYTES * i, reinterpret_cast<uint8_t *>(tx_data_structs[i]),
             N_STAT_ELEM_BYTES);
    }
    //
    if (spotter_tx_data(tx_data, N_TX_DATA_BYTES, BmNetworkTypeCellularIriFallback)) {
      printf("%llut - %s | Sucessfully sent Spotter transmit data request\n", uptimeGetMs(),
             rtcTimeBuffer);
    } else {
      printf("%llut - %s | Failed to send Spotter transmit data request\n", uptimeGetMs(),
             rtcTimeBuffer);
    }
  }

  if (pressure_stats.getNumSamples() >= MAX_SENSOR_SAMPLES) {
    printf("ERR - No more room in pressure stats buffer, already have %lu readings!\n",
           MAX_SENSOR_SAMPLES);
  } else {
    float temperature, pressure = 0.0;
    if (pressureSamplerGetLatest(pressure, temperature)) {
      pressure_stats.addSample(pressure);
      printf("pressure stats | count: %u/%lu, min: %f, max: %f\n",
             pressure_stats.getNumSamples(), MAX_SENSOR_SAMPLES - 10, pressure_stats.getMin(),
             pressure_stats.getMax());
    }
  }

  if (hum_stats.getNumSamples() >= MAX_SENSOR_SAMPLES) {
    printf("ERR - No more room in hum/temp stats buffer, already have %lu readings!\n",
           MAX_SENSOR_SAMPLES);
  } else {
    float temperature, humidity = 0.0;
    if (htuSamplerGetLatest(humidity, temperature)) {
      hum_stats.addSample(humidity);
      temp_stats.addSample(temperature);
      printf("hum-temp stats | count: %u/%lu, min_T: %f, max_T: %f, min_H: %f, "
             "max_H: %f\n",
             hum_stats.getNumSamples(), MAX_SENSOR_SAMPLES - 10, temp_stats.getMin(),
             temp_stats.getMax(), hum_stats.getMin(), hum_stats.getMax());
    }
  }

  if (power_voltage_stats.getNumSamples() >= MAX_SENSOR_SAMPLES) {
    printf("ERR - No more room in power stats buffer, already have %lu "
           "readings!\n",
           MAX_SENSOR_SAMPLES);
  } else {
    float voltage_mote, current_mote = 0.0;
    if (powerSamplerGetLatest(Bristlefin::I2C_ADDR_MOTE_THROUGH_POWER_MON, voltage_mote,
                              current_mote)) {
      power_voltage_stats.addSample(voltage_mote);
      power_current_stats.addSample(current_mote);

      printf("power stats | count: %u/%lu, min_V: %f, max_V: %f, min_I: %f, max_I: "
             "%f\n",
             power_voltage_stats.getNumSamples(), MAX_SENSOR_SAMPLES - 10,
             power_voltage_stats.getMin(), power_voltage_stats.getMax(),
             power_current_stats.getMin(), power_current_stats.getMax());
    }
  }

  /// This section demonstrates a simple non-blocking bare metal method for rollover-safe timed tasks,
  ///   like blinking an LED.
  /// More canonical (but more arcane) modern methods of implementing this kind functionality
  ///   would bee to use FreeRTOS tasks or hardware timer ISRs.
  static uint32_t ledPulseTimer = uptimeGetMs();
  static uint32_t ledOnTimer = 0;
  static bool ledState = false;
  // Turn LED1 on green every LED_PERIOD_MS milliseconds.
  if (!ledState && ((uint32_t)uptimeGetMs() - ledPulseTimer >= LED_PERIOD_MS)) {
    bristlefin.setLed(1, Bristlefin::LED_GREEN);
    ledOnTimer = uptimeGetMs();
    ledPulseTimer += LED_PERIOD_MS;
    ledState = true;
  }
  // If LED1 has been on for LED_ON_TIME_MS milliseconds, turn it off.
  else if (ledState && ((uint32_t)uptimeGetMs() - ledOnTimer >= LED_ON_TIME_MS)) {
    bristlefin.setLed(1, Bristlefin::LED_OFF);
    ledState = false;
  }
}
