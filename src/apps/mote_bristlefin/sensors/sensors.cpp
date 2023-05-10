#include <stdbool.h>
#include <stdint.h>
#include "bsp.h"
#include "sensors.h"

// Sensor driver includes
#include "ms5803.h"
#include "htu21d.h"
#include "ina232.h"

// Sampler initialization functions (so we don't need individual headers)
void powerSamplerInit(INA::INA232 **sensors);
void pressureSamplerInit(MS5803 *sensor);
void htuSamplerInit(HTU21D *sensor);

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
