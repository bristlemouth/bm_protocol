/*!
  Pressure sensor sampling functions
*/
#include "pressureSampler.h"
#include "abstract_pressure_sensor.h"
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

static AbstractPressureSensor *_pressureSensor;
static float _latestPressure = 0.0;
static float _latestTemperature = 0.0;
static bool _newPressureDataAvailable = false;
// Optional user-defined sample processing function
void userPressureSample(pressureSample_t pressure_sample) __attribute__((weak));

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
  } while (!success && (--retriesRemaining > 0));

  if (success) {
    RTCTimeAndDate_t time_and_date = {};
    rtcGet(&time_and_date);
    pressureSample_t _pressureData{.uptime = uptimeGetMs(),
                                   .rtcTime = time_and_date,
                                   .temperature = temperature,
                                   .pressure = pressure};

    char rtcTimeBuffer[32] = {};
    rtcPrint(rtcTimeBuffer, &time_and_date);
    printf("pressure | tick: %llu, rtc: %s, temp: %f, pressure: %f\n", uptimeGetMs(),
           rtcTimeBuffer, _pressureData.temperature, _pressureData.pressure);
    spotter_log(0, "pressure.log", USE_TIMESTAMP, "tick: %llu, rtc: %s, temp: %f, pressure: %f\n",
               uptimeGetMs(), rtcTimeBuffer, _pressureData.temperature, _pressureData.pressure);
    spotter_log_console(0, "pressure | tick: %llu, rtc: %s, temp: %f, pressure: %f", uptimeGetMs(),
              rtcTimeBuffer, _pressureData.temperature, _pressureData.pressure);

    taskENTER_CRITICAL();
    _latestPressure = pressure;
    _latestTemperature = temperature;
    _newPressureDataAvailable = true;
    taskEXIT_CRITICAL();
  } else {
    printf("ERR Failed to sample pressure sensor!");
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
  .initFn = baroInit,
  .sampleFn = baroSample,
  .checkFn = baroCheck
};


void pressureSamplerInit(AbstractPressureSensor *sensor) {
  _pressureSensor = sensor;
  sensorSamplerAdd(&pressureSensor, "BARO");
}

bool pressureSamplerGetLatest(float &pressure, float &temperature) {
  bool rval = false;

  taskENTER_CRITICAL();
  if (_newPressureDataAvailable) {
    pressure = _latestPressure;
    temperature = _latestTemperature;
    rval = true;
    _newPressureDataAvailable = false;
  }
  taskEXIT_CRITICAL();

  return rval;
}
