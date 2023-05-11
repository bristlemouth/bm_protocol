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

/*
  sensorSampler function to take barometer sample

  \return true if successful false otherwise
*/
static bool baroSample() {
  float temperature, pressure;
  bool success = false;
  uint8_t retriesRemaining = SENSORS_NUM_RETRIES;
  const char baroTopic[] = "pressure";

  do {
    success = _pressureSensor->readPTRaw(pressure, temperature);
  } while(!success && (--retriesRemaining > 0));

  if(success) {
    // pub to pressure topic
    bm_pub_t publication;

    int data_len = sizeof(float);

    publication.topic = const_cast<char *>(baroTopic);
    publication.topic_len = sizeof(baroTopic) - 1 ; // Don't care about Null terminator

    publication.data = reinterpret_cast<char *>(&pressure);
    publication.data_len = data_len; // Don't care about Null terminator

    bm_pubsub_publish(&publication);
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
