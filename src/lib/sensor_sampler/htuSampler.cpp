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

static void publish_float(const char *topic, float &value, uint8_t type, uint8_t version) {
  bm_pub(topic, &value, sizeof(float), type, version);
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
  static constexpr uint8_t humidityTopicType = 1;
  static constexpr uint8_t humidityTopicVersion = 1;
  const char temperatureTopic[] = "temperature";
  static constexpr uint8_t temperatureTopicType = 1;
  static constexpr uint8_t temperatureTopicVersion = 1;
  do {
    success = _htu21d->read(temperature, humidity);
  } while(!success && (--retriesRemaining > 0));

  if(success) {
    publish_float(humidityTopic, humidity, humidityTopicType, humidityTopicVersion);
    publish_float(temperatureTopic, temperature,temperatureTopicType,temperatureTopicVersion);
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
