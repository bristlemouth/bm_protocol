/*!
  Power sensor(s) sampling functions
*/

#include "bm_pubsub.h"
#include "bm_printf.h"
#include "bsp.h"
#include "uptime.h"
#include "debug.h"
#include "ina232.h"
#include "sensors.h"
#include "sensorSampler.h"
#include <stdbool.h>
#include <stdint.h>

using namespace INA;

static INA232** _inaSensors;

// Optional user-defined sample processing function
void userPowerSample(powerSample_t power_sample) __attribute__((weak));

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
      RTCTimeAndDate_t time_and_date = {};
      rtcGet(&time_and_date);
      powerSample_t _powerData {
          .uptime = uptimeGetMs(),
          .rtcTime = time_and_date,
          .address = _inaSensors[dev_num]->getAddr(),
          .voltage = voltage,
          .current = current
      };

      char rtcTimeBuffer[32];
      rtcPrint(rtcTimeBuffer, &time_and_date);
      printf("power | tick: %llu, rtc: %s, addr: %u, voltage: %f, current: %f\n", uptimeGetMs(), rtcTimeBuffer, _powerData.address, _powerData.voltage, _powerData.current);
      bm_fprintf(0, "power.log", "tick: %llu, rtc: %s, addr: %lu, voltage: %f, current: %f\n", uptimeGetMs(), rtcTimeBuffer,  _powerData.address, _powerData.voltage, _powerData.current);
      bm_printf(0, "power | tick: %llu, rtc: %s, addr: %u, voltage: %f, current: %f", uptimeGetMs(), rtcTimeBuffer, _powerData.address, _powerData.voltage, _powerData.current);
      if (userPowerSample) userPowerSample(_powerData);
    }
    else {
      printf("ERR Failed to sample power monitr %u!", dev_num);
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
  .initFn = powerInit,
  .sampleFn = powerSample,
  .checkFn = NULL
};

void powerSamplerInit(INA::INA232 **sensors) {
  _inaSensors = sensors;
  sensorSamplerAdd(&powerSensors, "PWR");
}
