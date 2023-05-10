/*!
  Temperature/Humidity sensor sampling functions
*/

#include "bm_pubsub.h"
#include "bsp.h"
#include "debug.h"
#include "htu21d.h"
#include "sensors.h"
#include "sensorSampler.h"
#include <stdbool.h>
#include <stdint.h>

static HTU21D* _htu21d;

#define HTU_STR_LEN 50

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
  const char printfTopic[] = "printf";

  do {
    success = _htu21d->read(temperature, humidity);
  } while(!success && (--retriesRemaining > 0));

  if(success) {
    // pub to printf or log topic
    bm_pub_t publication;

    char data[HTU_STR_LEN];

    int data_len = snprintf(data, HTU_STR_LEN, "humidity %f, temp %f\n", humidity, temperature);

    publication.topic = const_cast<char *>(printfTopic);
    publication.topic_len = sizeof(printfTopic) - 1 ; // Don't care about Null terminator

    publication.data = const_cast<char *>(data);
    publication.data_len = data_len - 1; // Don't care about Null terminator

    bm_pubsub_publish(&publication);
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
