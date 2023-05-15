/*!
  Power sensor(s) sampling functions
*/

#include "bm_pubsub.h"
#include "bsp.h"
#include "debug.h"
#include "ina232.h"
#include "sensors.h"
#include "sensorSampler.h"
#include <stdbool.h>
#include <stdint.h>

static INA::INA232** _inaSensors;

#define INA_STR_LEN 80

/*
  sensorSampler function to take power sample(s)

  \return true if successful false otherwise
*/
static bool powerSample() {
  float voltage, current;
  bool success = false;
  bool rval = true;
  uint8_t retriesRemaining = SENSORS_NUM_RETRIES;
  const char powerTopic[] = "power";
  for (uint8_t dev_num = 0; dev_num < NUM_INA232_DEV; dev_num++){
    do {
      success = _inaSensors[dev_num]->measurePower();
      if (success) {
        _inaSensors[dev_num]->getPower(voltage, current);
      }
    } while( !success && (--retriesRemaining > 0));

    if(success) {
      // pub to power topic
      bm_pub_t publication;

      struct {
        uint16_t address;
        float voltage;
        float current;
      } __attribute__((packed)) _powerData;

      _powerData.address = _inaSensors[dev_num]->getAddr();
      _powerData.voltage = voltage;
      _powerData.current = current;

      publication.topic = const_cast<char *>(powerTopic);
      publication.topic_len = sizeof(powerTopic) - 1 ; // Don't care about Null terminator

      publication.data = reinterpret_cast<char *>(&_powerData);
      publication.data_len = sizeof(_powerData);

      bm_pubsub_publish(&publication);
    }
    rval &= success;
  }
  return rval;
}

/*
  sensorSampler function to initialize the power monitor('s)

  \return true if successful false otherwise
*/
static bool powerInit() {
  bool rval = true;
  for (uint8_t dev_num = 0; dev_num < NUM_INA232_DEV; dev_num++){
    rval &= _inaSensors[dev_num]->init();
  }
  return rval;
}

static sensor_t powerSensors = {
  .intervalMs = 1000,
  .initFn = powerInit,
  .sampleFn = powerSample,
  .checkFn = NULL
};

void powerSamplerInit(INA::INA232 **sensors) {
  _inaSensors = sensors;
  sensorSamplerAdd(&powerSensors, "PWR");
}
