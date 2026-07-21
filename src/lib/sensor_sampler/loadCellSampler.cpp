#include "loadCellSampler.h"
#include "spotter.h"
#include "pubsub.h"
#include "bsp.h"
#include "debug.h"
#include "sensorSamplerConfigs.h"
#include "sensorSampler.h"
#include "sensors.h"
#include "stm32_rtc.h"
#include "uptime.h"
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>


static NAU7802 *_loadCell;

static bool negative_factor = false;

static uint32_t reading_attempts_counter = 0; //Counts number of readings or reading attempts taken. compared against num reads to determine when to send a message
static uint32_t sucessful_readings_counter = 0; //This is the number of sucessful readings. This allows accurate means and variance to be calcuated while still counting missed reading separately. 
static uint32_t missed_reading_counter = 0;

static float mean_force = 0;
static float sum_of_weights = 0;
static float max_force = 0;
static float min_force = 10000;

static float running_mean = 0;
static float running_m2 = 0;
static float old_mean = 0;
static float stdev = 0;
static float variance = 0;

static uint32_t num_reads = 570; //at one reading per 500 ms, this should be 4.75 minutes. 
static LoadCellConfig_t _cfg;

/*
  sensorSampler function to take power sample(s)
  \return true if successful false otherwise
  We are calling begin from the sample function. We want this to be the one that reads the data.
  Right now, check, init, and sample all just do begin. Need to look at other sensors for a template:
  init    -- assume this is good for now
  sample  --for the NAU7802, this funnels down to
  check   -- For Power and HTU, this is NULL. for baro, it does a checkPROM which funnels down to a readData command. Pretty much just check if you get a reading. We can leave it blank for now.
*/


static bool loadCellSample() {
  printf("Load cell sample called\n"); //mote debug line

  //function specific definitions
  bool rval = true;
  static int32_t reading = 0;
  static float weight = 0;
  static float calFactor = 0;
  static int32_t zeroOffset = 0;

  RTCTimeAndDate_t timeAndDate;
  char rtcTimeBuffer[32];
  if (rtcGet(&timeAndDate) == pdPASS) {
    sprintf(rtcTimeBuffer, "%04u-%02u-%02uT%02u:%02u:%02u.%03u", timeAndDate.year,
            timeAndDate.month, timeAndDate.day, timeAndDate.hour, timeAndDate.minute,
            timeAndDate.second, timeAndDate.ms);
  } else {
    strcpy(rtcTimeBuffer, "0");
  }

  //Load cell read block (if LC is readable)
  if (_loadCell->available()) {
    
    // Reading and overwriting "current" data values
    reading = _loadCell->getReading();
    weight = _loadCell->getWeight(negative_factor);
    calFactor = _loadCell->getCalibrationFactor();
    zeroOffset = _loadCell->getZeroOffset();
  
    //updates to counter values
    reading_attempts_counter ++; // This one is used to trigger the end of the reading period and send a message. 
    sucessful_readings_counter ++; // This one is used to calculate accurate running stats. If the loadcell never fails to read, this will equal cellular_sen_read_counter
    
    //Updates to Mix, Max, running mean and variance. 
    if (weight < min_force) {
      min_force = weight;
    }
    if (weight > max_force) {
      max_force = weight;
    }
    sum_of_weights += weight; // for period-mean at the end
    old_mean = running_mean;
    running_mean = running_mean + ((weight - running_mean)/sucessful_readings_counter);
    running_m2 = running_m2 + ((weight - running_mean)*(weight - old_mean));  


    //Debugging stuff that prints on mote 
    printf("%llu | reading: %ld\n", uptimeGetMicroSeconds() / 1000, reading);
    _loadCell->getInternalOffsetCal(); // In this function there are calls to printf the three bytes of the internal offset cal on the mote serial. This is a leftover from the public arduino libary. someday, the nau7802 lib should get refactors so that there are not buried print calls and we just return the values. 
    printf("%llu | weight: %f\n", uptimeGetMicroSeconds() / 1000, weight);
    printf("%llu | calFactor: %f\n", uptimeGetMicroSeconds() / 1000, calFactor);
    printf("%llu | zeroOffset: %ld\n", uptimeGetMicroSeconds() / 1000, zeroOffset);
  
    // print commands to SD card file and spotter console.
    spotter_log(0, "loadcell.log", USE_TIMESTAMP,
                "tick: %llu, rtc: %s, reading: %" PRId32 "\n", uptimeGetMicroSeconds() / 1000,
                rtcTimeBuffer, reading);
    spotter_log(0, "loadcell.log", USE_TIMESTAMP, "tick: %llu, rtc: %s, weight: %f\n",
                uptimeGetMicroSeconds() / 1000, rtcTimeBuffer, weight);
    spotter_log_console(0, "loadcell | tick: %llu, rtc: %s, reading: %" PRId32 "\n",
              uptimeGetMicroSeconds() / 1000, rtcTimeBuffer, reading);
    spotter_log_console(0, "loadcell | tick: %llu, rtc: %s, weight: %f\n",
              uptimeGetMicroSeconds() / 1000, rtcTimeBuffer, weight);

  } 
  
  //If load cell read fails 
  else {
    missed_reading_counter ++; // to track at the end
    reading_attempts_counter++; //This still updates so we send our message on time. We do not interate successful_reading_count. 
    // none of our values need to update here. 
  }
  
  // Message send block once desired reading-attempt-count is reached.  
  if (reading_attempts_counter % num_reads == 0) {
    printf("\n num_reads reached. Sending a cellular message. "); // debug line
    
    if (sucessful_readings_counter > 0) { // avoids divide by zero error if all the LC readings fail. Not sure what would happen. 
      mean_force = sum_of_weights / sucessful_readings_counter;
      variance   = running_m2 / sucessful_readings_counter;   // population variance not sample variance
      stdev      = sqrtf(variance);
    } 
    else { // in case no successful readings. 
      mean_force = 0.0f;
      variance   = 0.0f;
      stdev      = 0.0f;
    }

    // Remote message send
    char data_string[300]; // made this a little bigger to accomdate new values. 
    memset(data_string, 0, sizeof(data_string));
    printf("LOAD_00, rtc: %s  | mean: %f |  max: %f  | min: %f  | stdev: %f  | readings: %" PRIu32 "  | missed readings: %" PRIu32 "\n",
       rtcTimeBuffer, mean_force, max_force, min_force, stdev,
       sucessful_readings_counter, missed_reading_counter);
    sprintf(data_string,
         "LOAD_00, rtc: %s  | mean: %f |  max: %f  | min: %f  | stdev: %f  | readings: %" PRIu32 "  | missed readings: %" PRIu32 "\n",
         rtcTimeBuffer, mean_force, max_force, min_force, stdev,
         sucessful_readings_counter, missed_reading_counter);
    spotter_tx_data(data_string, strlen(data_string), BmNetworkTypeCellularIriFallback);


    //prints lines in SD and console to indicate remote message send
    spotter_log(0, "loadcell.log", USE_TIMESTAMP, "Loadcell reading period ended.\n");
    spotter_log(0, "loadcell.log", USE_TIMESTAMP, data_string);
    spotter_log_console(0, "Loadcell reading period ended.");
    spotter_log_console(0, data_string);


    //Resetting all the counters and stats for the next cycle. 
    reading_attempts_counter = 0;
    sucessful_readings_counter = 0;
    missed_reading_counter = 0;
    
    sum_of_weights = 0;
    max_force = 0;
    min_force = 10000;
    mean_force = 0;

    running_mean = 0;
    running_m2 = 0;
    variance = 0;
    old_mean = 0;
    stdev = 0;
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
  if (_cfg.calibration_factor < 0.0) {
      negative_factor = true;
  }

  printf("loadCell init rval: %d \n", rval);
  return rval;
}

static bool loadCellCheck() {
  //eventually just put a readData type call in here.
  printf("loadCell check called\n");

  return 1;
}

static sensor_t loadCellSensor = {
    .intervalMs = DEFAULT_SENSORS_POLL_MS, .initFn = loadCellInit, .sampleFn = loadCellSample, .checkFn = loadCellCheck};

void loadCellSamplerInit(NAU7802 *sensor, LoadCellConfig_t cfg) {
  _loadCell = sensor;
  _cfg = cfg;
  get_sensor_poll_interval_ms(&loadCellSensor);
  sensorSamplerAdd(&loadCellSensor, "LCL");
}
