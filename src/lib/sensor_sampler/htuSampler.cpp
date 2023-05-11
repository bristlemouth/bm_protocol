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


static void publish_float(const char *topic, size_t topic_len, float *value) {
  // pub to topic
  bm_pub_t publication;

  publication.topic = const_cast<char *>(topic);
  publication.topic_len = topic_len - 1 ; // Don't care about Null terminator

  publication.data = reinterpret_cast<char *>(value);
  publication.data_len = sizeof(float); // Don't care about Null terminator

  bm_pubsub_publish(&publication);
}

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
  const char humidityTopic[] = "humidity";
  const char temperatureTopic[] = "temperature";

  do {
    success = _htu21d->read(temperature, humidity);
  } while(!success && (--retriesRemaining > 0));

  if(success) {
    publish_float(humidityTopic, sizeof(humidityTopic), &humidity);
    publish_float(temperatureTopic, sizeof(temperatureTopic), &temperature);
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
