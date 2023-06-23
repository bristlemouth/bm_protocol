/*!
  Power sensor(s) sampling functions
*/

#include "bm_pubsub.h"
#include "bm_printf.h"
#include "bsp.h"
#include "debug.h"
#include "ina232.h"
#include "sensors.h"
#include "sensorSampler.h"
#include <stdbool.h>
#include <stdint.h>

using namespace INA;

static INA232** _inaSensors;

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

  for (uint8_t dev_num = 0; dev_num < NUM_INA232_DEV; dev_num++){
    do {
      success = _inaSensors[dev_num]->measurePower();
      if (success) {
        _inaSensors[dev_num]->getPower(voltage, current);
      }
    } while( !success && (--retriesRemaining > 0));

    if(success) {

      struct {
        uint16_t address;
        float voltage;
        float current;
      } __attribute__((packed)) _powerData;

      _powerData.address = _inaSensors[dev_num]->getAddr();
      _powerData.voltage = voltage;
      _powerData.current = current;

      bm_fprintf(0, "power.log",
                 "addr: %lu, voltage: %f, current: %f\n", _powerData.address, _powerData.voltage, _powerData.current);
      bm_printf(0,
                 "power | addr: %u, voltage: %f, current: %f\n", _powerData.address, _powerData.voltage, _powerData.current);
      printf("power | addr: %u, voltage: %f, current: %f\n", _powerData.address, _powerData.voltage, _powerData.current);
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
    if (_inaSensors[dev_num]->init()){
      // set shunt to 10mOhms
      _inaSensors[dev_num]->setShuntValue(0.01);

      //Set normal sampling speed
      _inaSensors[dev_num]->setBusConvTime(CT_1100);
      _inaSensors[dev_num]->setShuntConvTime(CT_1100);
      _inaSensors[dev_num]->setAvg(AVG_256);
      rval &= true;
    }
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
