/*!
  Temperature/Humidity sensor sampling functions
*/

#include "bm_pubsub.h"
#include "bm_printf.h"
#include "bsp.h"
#include "debug.h"
#include "htu21d.h"
#include "sensors.h"
#include "sensorSampler.h"
#include <stdbool.h>
#include <stdint.h>

static HTU21D* _htu21d;

/*
  sensorSampler function to initialize HTU

  \return true if successful false otherwise
*/
static bool htuInit() {
  return _htu21d->init();
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
    success = _htu21d->read(temperature, humidity);
  } while(!success && (--retriesRemaining > 0));

  if(success) {
    bm_fprintf(0, "hum_temp.log", "hum: %f, temp: %f\n", humidity, temperature);
    bm_printf(0, "htu | hum: %f, temp: %f\n", humidity, temperature);
    printf("htu | hum: %f, temp: %f\n", humidity, temperature);
  }

  return success;
}

static sensor_t htuSensor = {
  .intervalMs = 1000,
  .initFn = htuInit,
  .sampleFn = htuSample,
  .checkFn = NULL
};

void htuSamplerInit(HTU21D *sensor) {
  _htu21d = sensor;
  sensorSamplerAdd(&htuSensor, "HTU");
}
