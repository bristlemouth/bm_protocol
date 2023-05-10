/*!
  Pressure sensor sampling functions
*/

#include "bm_pubsub.h"
#include "bsp.h"
#include "debug.h"
#include "ms5803.h"
#include "sensors.h"
#include "sensorSampler.h"
#include <stdbool.h>
#include <stdint.h>

static MS5803* _pressureSensor;

#define PRESSURE_STR_LEN 50

static bool baroSample() {
  float temperature, pressure;
  bool success = false;
  uint8_t retriesRemaining = SENSORS_NUM_RETRIES;
  const char printfTopic[] = "printf";

  do {
    success = _pressureSensor->readPTRaw(pressure, temperature);
  } while(!success && (--retriesRemaining > 0));

  if(success) {
    // pub to printf or log topic
    bm_pub_t publication;

    char data[PRESSURE_STR_LEN];

    int data_len = snprintf(data, PRESSURE_STR_LEN, "pressure %f, temp %f\n", pressure, temperature);

    publication.topic = const_cast<char *>(printfTopic);
    publication.topic_len = sizeof(printfTopic) - 1 ; // Don't care about Null terminator

    publication.data = const_cast<char *>(data);
    publication.data_len = data_len - 1; // Don't care about Null terminator

    bm_pubsub_publish(&publication);
  }

  return success;
}
static bool baroInit() {
  return _pressureSensor->init();
}

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
