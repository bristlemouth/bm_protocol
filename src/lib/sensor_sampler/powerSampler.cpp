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

static bool powerSample() {
  float voltage, current;
  bool success = false;
  bool rval = true;
  uint8_t retriesRemaining = SENSORS_NUM_RETRIES;
  const char printfTopic[] = "printf";
  for (uint8_t dev_num = 0; dev_num < NUM_INA232_DEV; dev_num++){
    do {
      success = _inaSensors[dev_num]->measurePower();
      if (success) {
        _inaSensors[dev_num]->getPower(voltage, current);
      }
    } while( !success && (--retriesRemaining > 0));

    if(success) {
      // pub to printf or log topic
      bm_pub_t publication;

      char data[INA_STR_LEN];

      int data_len = snprintf(data, INA_STR_LEN, "dev_addr: %" PRIx16 " voltage %f, current %f, power %f\n", _inaSensors[dev_num]->getAddr(), voltage, current, voltage*current);

      publication.topic = const_cast<char *>(printfTopic);
      publication.topic_len = sizeof(printfTopic) - 1 ; // Don't care about Null terminator

      publication.data = const_cast<char *>(data);
      publication.data_len = data_len - 1; // Don't care about Null terminator

      bm_pubsub_publish(&publication);
    }
    rval &= success;
  }
  return rval;
}

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
