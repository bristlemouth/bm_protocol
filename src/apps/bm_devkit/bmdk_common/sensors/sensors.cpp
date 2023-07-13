#include <stdbool.h>
#include <stdint.h>
#include "bsp.h"
#include "sensors.h"

// Sensor driver includes
#include "ms5803.h"
#include "htu21d.h"
#include "ina232.h"

// Sampler initialization functions (so we don't need individual headers)
void powerSamplerInit(INA::INA232 **sensors); // implemented in src/lib/sensor_sampler/powerSampler.cpp
void pressureSamplerInit(MS5803 *sensor); // src/lib/sensor_sampler/pressureSampler.cpp
void htuSamplerInit(HTU21D *sensor); // src/lib/sensor_sampler/htuSampler.cpp

MS5803 debugPressure(&i2c1, MS5803_ADDR);
HTU21D debugHTU(&i2c1);
INA::INA232 debugIna1(&i2c1, I2C_INA_MAIN_ADDR);
INA::INA232 debugIna2(&i2c1, I2C_INA_PODL_ADDR);
INA::INA232 *debugIna[NUM_INA232_DEV] = {
  &debugIna1,
  &debugIna2,
};

void sensorsInit() {

  // Power monitor
  powerSamplerInit(debugIna);

  // Initialize temperature/humidity
  htuSamplerInit(&debugHTU);

  // Initialize Barometer sampling
  pressureSamplerInit(&debugPressure);

}
