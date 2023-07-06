/*!
  Pressure sensor sampling functions
*/

#include "bm_pubsub.h"
#include "bm_printf.h"
#include "bsp.h"
#include "debug.h"
#include "ms5803.h"
#include "stm32_rtc.h"
#include "uptime.h"
#include "sensors.h"
#include "sensorSampler.h"
#include <stdbool.h>
#include <stdint.h>

static MS5803* _pressureSensor;

/*
  sensorSampler function to take barometer sample

  \return true if successful false otherwise
*/
static bool baroSample() {
  float temperature, pressure;
  bool success = false;
  uint8_t retriesRemaining = SENSORS_NUM_RETRIES;

  do {
    success = _pressureSensor->readPTRaw(pressure, temperature);
  } while(!success && (--retriesRemaining > 0));

  if(success) {
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

    bm_fprintf(0, "pressure.log", "tick: %llu, rtc: %s, temp: %f, pressure: %f\n", uptimeGetMicroSeconds()/1000, rtcTimeBuffer, temperature, pressure);
    bm_printf(0, "pressure | tick: %llu, rtc: %s, temp: %f, pressure: %f", uptimeGetMicroSeconds()/1000, rtcTimeBuffer, temperature, pressure);
    printf("pressure | tick: %llu, rtc: %s, temp: %f, pressure: %f\n", uptimeGetMicroSeconds()/1000, rtcTimeBuffer, temperature, pressure);
  }

  return success;
}

/*
  sensorSampler function to initialize the barometer

  \return true if successful false otherwise
*/
static bool baroInit() {
  return _pressureSensor->init();
}

/*
  sensorSampler function to check the barometer

  \return true if successful false otherwise
*/
static bool baroCheck() {
  return _pressureSensor->checkPROM();
}

static sensor_t pressureSensor = {
  .intervalMs = 1000,
  .initFn = baroInit,
  .sampleFn = baroSample,
  .checkFn = baroCheck
};


void pressureSamplerInit(MS5803 *sensor) {
  _pressureSensor = sensor;
  sensorSamplerAdd(&pressureSensor, "BARO");
}
