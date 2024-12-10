/*!
  Load cell sampling functions
*/

#include "loadCellSampler.h"
#include "spotter.h"
#include "pubsub.h"
#include "bsp.h"
#include "debug.h"
#include "sensorSampler.h"
#include "sensors.h"
#include "stm32_rtc.h"
#include "uptime.h"
#include <stdbool.h>
#include <stdint.h>

static NAU7802 *_loadCell;
bool successful_lc_read = false;
uint64_t read_attempt_duration = 100;
uint64_t read_start_time;

uint32_t cellular_send_read_counter;
float mean_force;
float mean_sum;
float max_force;
float min_force;
uint32_t num_reads = 240; //This should be 4 minutes
LoadCellConfig_t _cfg;

#define INA_STR_LEN 80

/*
  sensorSampler function to take power sample(s)
  \return true if successful false otherwise
  We are calling begin from the sample function. We want this to be the one that reads the data.
  Right now, check, init, and sample all just do begin. Need to look at other sensors for a template:
  init    -- assume this is good for now
  sample  --for the NAU7802, this funnels down to
  check   -- For Power and HTU, this is NULL. for baro, it does a checkPROM which funnels down to a readData command. Pretty much just check if you get a reading. We can leave it blank for now.
*/

/* Checking with a soak.  */
static bool loadCellSample() {
  printf("Load cell sample called\n");

  bool rval = true;
  int32_t reading = _loadCell->getReading();
  float weight = _loadCell->getWeight();
  float calFactor = _loadCell->getCalibrationFactor();
  int32_t zeroOffset = _loadCell->getZeroOffset();

  RTCTimeAndDate_t timeAndDate;
  char rtcTimeBuffer[32];
  if (rtcGet(&timeAndDate) == pdPASS) {
    sprintf(rtcTimeBuffer, "%04u-%02u-%02uT%02u:%02u:%02u.%03u", timeAndDate.year,
            timeAndDate.month, timeAndDate.day, timeAndDate.hour, timeAndDate.minute,
            timeAndDate.second, timeAndDate.ms);
  } else {
    strcpy(rtcTimeBuffer, "0");
  }
  successful_lc_read = false;
  read_start_time = uptimeGetMicroSeconds() / 1000;
  while ((!successful_lc_read) &&
         (((uptimeGetMicroSeconds() / 1000) - read_start_time) < read_attempt_duration)) {
    if (_loadCell->available()) {
      printf("%llu | reading: %d\n", uptimeGetMicroSeconds() / 1000, reading);
      // printf("%llu | reading corrs: %d\n", uptimeGetMicroSeconds()/1000, reading-333900);
      _loadCell->getInternalOffsetCal();

      // prints to SD card file
      spotter_log(0, "loadcell.log", USE_TIMESTAMP,
                 "tick: %llu, rtc: %s, reading: %" PRId32 "\n", uptimeGetMicroSeconds() / 1000,
                 rtcTimeBuffer, reading);
      spotter_log(0, "loadcell.log", USE_TIMESTAMP, "tick: %llu, rtc: %s, weight: %f\n",
                 uptimeGetMicroSeconds() / 1000, rtcTimeBuffer, weight);

      // prints to Spotter console
      spotter_log_console(0, "loadcell | tick: %llu, rtc: %s, reading: %" PRId32 "\n",
                uptimeGetMicroSeconds() / 1000, rtcTimeBuffer, reading);
      spotter_log_console(0, "loadcell | tick: %llu, rtc: %s, weight: %f\n",
                uptimeGetMicroSeconds() / 1000, rtcTimeBuffer, weight);
      printf("%llu | weight: %f\n", uptimeGetMicroSeconds() / 1000, weight);
      printf("%llu | calFactor: %f\n", uptimeGetMicroSeconds() / 1000, calFactor);
      printf("%llu | zeroOffset: %d\n", uptimeGetMicroSeconds() / 1000, zeroOffset);
      successful_lc_read = true;
    } else {
      vTaskDelay(pdMS_TO_TICKS(10));
    }
  }
  cellular_send_read_counter++;
  if (weight < min_force) {
    min_force = weight;
  }
  if (weight > max_force) {
    max_force = weight;
  }
  mean_sum += weight;

  if (cellular_send_read_counter % num_reads == 0) {
    printf("\n\n\n\nThis should only print once every 4 mins\n\n\n");
    mean_force = mean_sum / cellular_send_read_counter;

    printf("mean force: %f |  max force: %f  | min force: %f\n\n\n", mean_force, max_force,
           min_force);

    char data_string[100];

    sprintf(data_string, "mean force: %f |  max force: %f  | min force: %f", mean_force,
            max_force, min_force);

    spotter_tx_data(data_string, 100, BmNetworkTypeCellularIriFallback);

    // printf(data_string);

    mean_sum = 0;
    cellular_send_read_counter = 0;
    max_force = 0;
    min_force = 10000;
    mean_force = 0;
  }

  return rval;
}

/*
  sensorSampler function to initialize the power monitor(s)
  \return true if successful false otherwise
*/
static bool loadCellInit() {
  bool rval = true;

  // Wait 3 seconds before doing the lc self cal in begin().
  vTaskDelay(pdMS_TO_TICKS(3000));
  rval = _loadCell->begin();
  _loadCell->setCalibrationFactor(_cfg.calibration_factor);
  _loadCell->setZeroOffset(_cfg.zero_offset);

  printf("loadCell init rval: %u\n", rval);
  return rval;

  // initial vals for force averaging.
  cellular_send_read_counter = 0;
  mean_force = 0;
  max_force = 0;
  min_force = 10000;
}

static bool loadCellCheck() {
  //eventually just put a readData type call in here.
  printf("loadCell check called\n");

  return 1;
}

static sensor_t loadCellSensor = {
    .initFn = loadCellInit, .sampleFn = loadCellSample, .checkFn = loadCellCheck};

void loadCellSamplerInit(NAU7802 *sensor, LoadCellConfig_t cfg) {
  _loadCell = sensor;
  _cfg = cfg;
  sensorSamplerAdd(&loadCellSensor, "LCL");
}
