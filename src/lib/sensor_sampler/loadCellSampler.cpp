/*!
  Power sensor(s) sampling functions
*/

#include "bm_pubsub.h"
#include "bm_printf.h"
#include "bsp.h"
#include "stm32_rtc.h"
#include "uptime.h"
#include "debug.h"
#include "nau7802.h"
#include "sensors.h"
#include "sensorSampler.h"
#include <stdbool.h>
#include <stdint.h>

static NAU7802* _loadCell;
bool successful_lc_read = false;
uint64_t read_attempt_duration = 100;
uint64_t read_start_time;

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
static bool loadCellSample() {
//  float voltage, current;
//  bool success = false;


  bool rval = true;
// //  uint8_t retriesRemaining = SENSORS_NUM_RETRIES;
//   rval = _loadCell->begin();
//   printf("loadCell sample rval: %u\n", rval);
  int32_t reading = _loadCell->getReading();
  float weight = _loadCell->getWeight();
  float calFactor = _loadCell->getCalibrationFactor();
  int32_t zeroOffset = _loadCell->getZeroOffset();

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
  successful_lc_read = false;
  read_start_time = uptimeGetMicroSeconds()/1000;
  while((!successful_lc_read) && (((uptimeGetMicroSeconds()/1000)-read_start_time)<read_attempt_duration))
  {
    if(_loadCell->available())
    {
      printf("%llu | reading: %d\n", uptimeGetMicroSeconds()/1000, reading);
      // printf("%llu | reading corrs: %d\n", uptimeGetMicroSeconds()/1000, reading-333900);
      _loadCell->getInternalOffsetCal();
      // prints to SD card file
      bm_fprintf(0, "loadcell.log", "tick: %llu, rtc: %s, reading: %lu\n",uptimeGetMicroSeconds()/1000, rtcTimeBuffer,  reading);
      bm_fprintf(0, "loadcell.log", "tick: %llu, rtc: %s, weight: %f\n",uptimeGetMicroSeconds()/1000, rtcTimeBuffer,  weight);

      // prints to Spotter console
      bm_printf(0, "loadcell | tick: %llu, rtc: %s, weight: %f", uptimeGetMicroSeconds()/1000, rtcTimeBuffer, weight);
      printf("%llu | weight: %f\n", uptimeGetMicroSeconds()/1000, weight);
      printf("%llu | calFactor: %f\n", uptimeGetMicroSeconds()/1000, calFactor);
      printf("%llu | zeroOffset: %d\n", uptimeGetMicroSeconds()/1000, zeroOffset);
      successful_lc_read = true;
    }
    else
    {
      vTaskDelay(pdMS_TO_TICKS(10));
    }
  }

/*
//  for (uint8_t dev_num = 0; dev_num < NUM_INA232_DEV; dev_num++){
//    do {
//      success = _inaSensors[dev_num]->measurePower();
//      if (success) {
//        _inaSensors[dev_num]->getPower(voltage, current);
//      }
//    } while( !success && (--retriesRemaining > 0));
//
//    if(success) {
//
//      struct {
//        uint16_t address;
//        float voltage;
//        float current;
//      } __attribute__((packed)) _powerData;
//
//      _powerData.address = _inaSensors[dev_num]->getAddr();
//      _powerData.voltage = voltage;
//      _powerData.current = current;
//
//      RTCTimeAndDate_t timeAndDate;
//      char rtcTimeBuffer[32];
//      if (rtcGet(&timeAndDate) == pdPASS) {
//        sprintf(rtcTimeBuffer, "%04u-%02u-%02uT%02u:%02u:%02u.%03u",
//                timeAndDate.year,
//                timeAndDate.month,
//                timeAndDate.day,
//                timeAndDate.hour,
//                timeAndDate.minute,
//                timeAndDate.second,
//                timeAndDate.ms);
//      } else {
//        strcpy(rtcTimeBuffer, "0");
//      }
//
//      bm_fprintf(0, "power.log", "tick: %llu, rtc: %s, addr: %lu, voltage: %f, current: %f\n",uptimeGetMicroSeconds()/1000, rtcTimeBuffer,  _powerData.address, _powerData.voltage, _powerData.current);
//      bm_printf(0, "power | tick: %llu, rtc: %s, addr: %u, voltage: %f, current: %f", uptimeGetMicroSeconds()/1000, rtcTimeBuffer, _powerData.address, _powerData.voltage, _powerData.current);
//      printf("power | tick: %llu, rtc: %s, addr: %u, voltage: %f, current: %f\n", uptimeGetMicroSeconds()/1000, rtcTimeBuffer, _powerData.address, _powerData.voltage, _powerData.current);
//    }
//    rval &= success;
//  }
*/
  return rval;
}

/*
  sensorSampler function to initialize the power monitor('s)
  \return true if successful false otherwise
*/
static bool loadCellInit() {
//  float voltage, current;
//  bool success = false;
  bool rval = true;
//  uint8_t retriesRemaining = SENSORS_NUM_RETRIES;
  vTaskDelay(pdMS_TO_TICKS(3000)); // Wait 3 seconds before doing the lc self cal in begin().
  rval = _loadCell->begin();
  //
  _loadCell->setCalibrationFactor(226.33);
  _loadCell->setZeroOffset(-17677.8);
  // printf("This aught to be getting callllled");





  printf("loadCell init rval: %u\n", rval);



/*
//  for (uint8_t dev_num = 0; dev_num < NUM_INA232_DEV; dev_num++){
//    do {
//      success = _inaSensors[dev_num]->measurePower();
//      if (success) {
//        _inaSensors[dev_num]->getPower(voltage, current);
//      }
//    } while( !success && (--retriesRemaining > 0));
//
//    if(success) {
//
//      struct {
//        uint16_t address;
//        float voltage;
//        float current;
//      } __attribute__((packed)) _powerData;
//
//      _powerData.address = _inaSensors[dev_num]->getAddr();
//      _powerData.voltage = voltage;
//      _powerData.current = current;
//
//      RTCTimeAndDate_t timeAndDate;
//      char rtcTimeBuffer[32];
//      if (rtcGet(&timeAndDate) == pdPASS) {
//        sprintf(rtcTimeBuffer, "%04u-%02u-%02uT%02u:%02u:%02u.%03u",
//                timeAndDate.year,
//                timeAndDate.month,
//                timeAndDate.day,
//                timeAndDate.hour,
//                timeAndDate.minute,
//                timeAndDate.second,
//                timeAndDate.ms);
//      } else {
//        strcpy(rtcTimeBuffer, "0");
//      }
//
//      bm_fprintf(0, "power.log", "tick: %llu, rtc: %s, addr: %lu, voltage: %f, current: %f\n",uptimeGetMicroSeconds()/1000, rtcTimeBuffer,  _powerData.address, _powerData.voltage, _powerData.current);
//      bm_printf(0, "power | tick: %llu, rtc: %s, addr: %u, voltage: %f, current: %f", uptimeGetMicroSeconds()/1000, rtcTimeBuffer, _powerData.address, _powerData.voltage, _powerData.current);
//      printf("power | tick: %llu, rtc: %s, addr: %u, voltage: %f, current: %f\n", uptimeGetMicroSeconds()/1000, rtcTimeBuffer, _powerData.address, _powerData.voltage, _powerData.current);
//    }
//    rval &= success;
//  }
*/
  return rval;
}

static bool loadCellCheck() {
  // rval = 1;
  //eventually just put a readData type call in here.
  printf("loadCell check called\n");

  return 1;
}

static sensor_t loadCellSensor = {
    .initFn = loadCellInit,
    .sampleFn = loadCellSample,
    .checkFn = loadCellCheck
};

void loadCellSamplerInit(NAU7802 *sensor) {
  _loadCell = sensor;
  sensorSamplerAdd(&loadCellSensor, "LCL");
}
