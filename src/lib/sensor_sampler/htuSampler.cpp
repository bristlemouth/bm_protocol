/*!
  Temperature/Humidity sensor sampling functions
*/

#include "abstract_htu_sensor.h"
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
#include "bm_printf.h"

static AbstractHtu *_htu;
static float _latestHumidity = 0.0;
static float _latestTemperature = 0.0;
static bool _newHumTempDataAvailable = false;

// Optional user-defined sample processing function
void userHumTempSample(humTempSample_t hum_temp_sample) __attribute__((weak));

/*
  sensorSampler function to initialize HTU
  \return true if successful false otherwise
*/
static bool htuInit() {
  return _htu->init();
}

/*
  sensorSampler function to take HTU sample

  \return true if successful false otherwise
*/
static bool htuSample() {
  float temperature, humidity;
  bool success = false;
  uint8_t retriesRemaining = SENSORS_NUM_RETRIES;

  do {
    success = _htu->read(temperature, humidity);
  } while (!success && (--retriesRemaining > 0));

  if (success) {
    RTCTimeAndDate_t time_and_date = {};
    rtcGet(&time_and_date);
    humTempSample_t _humTempData{.uptime = uptimeGetMs(),
                                 .rtcTime = time_and_date,
                                 .temperature = temperature,
                                 .humidity = humidity};

    char rtcTimeBuffer[32];
    rtcPrint(rtcTimeBuffer, &time_and_date);
    printf("hum_temp | tick: %llu, rtc: %s, hum: %f, temp: %f\n", uptimeGetMs(), rtcTimeBuffer,
           _humTempData.humidity, _humTempData.temperature);
    spotter_log(0, "hum_temp.log", USE_TIMESTAMP, "tick: %llu, rtc: %s, hum: %f, temp: %f\n", uptimeGetMs(),
               rtcTimeBuffer, _humTempData.humidity, _humTempData.temperature);
    spotter_log_console(0, "hum_temp | tick: %llu, rtc: %s, hum: %f, temp: %f", uptimeGetMs(),
              rtcTimeBuffer, _humTempData.humidity, _humTempData.temperature);

    taskENTER_CRITICAL();
    _latestHumidity = humidity;
    _latestTemperature = temperature;
    _newHumTempDataAvailable = true;
    taskEXIT_CRITICAL();
  } else {
    printf("ERR Failed to sample pressure sensor!");
  }
  return success;
}

static sensor_t htuSensor = {
  .initFn = htuInit,
  .sampleFn = htuSample,
  .checkFn = NULL
};

void htuSamplerInit(AbstractHtu *sensor) {
  _htu = sensor;
  sensorSamplerAdd(&htuSensor, "HTU");
}

bool htuSamplerGetLatest(float &humidity, float &temperature) {
  bool rval = false;
  taskENTER_CRITICAL();
  if (_newHumTempDataAvailable) {
    temperature = _latestTemperature;
    humidity = _latestHumidity;
    _newHumTempDataAvailable = false;
    rval = true;
  }
  taskEXIT_CRITICAL();
  return rval;
}
