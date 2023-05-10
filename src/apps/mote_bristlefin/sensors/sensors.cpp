#include <stdbool.h>
#include <stdint.h>
#include "bsp.h"
#include "sensors.h"

// Sensor driver includes
#include "ms5803.h"
#include "htu21d.h"

// Sampler initialization functions (so we don't need individual headers)
// void powerSamplerInit();
void pressureSamplerInit(MS5803 *sensor);
void htuSamplerInit(HTU21D *sensor);

MS5803 debugPressure(&i2c1, MS5803_ADDR);
HTU21D debugHTU(&i2c1);

void sensorsInit() {

  // TODO - implement this
  // Power monitor
  // powerSamplerInit();

  // Initialize temperature/humidity
  htuSamplerInit(&debugHTU);

  // Initialize Barometer sampling
  pressureSamplerInit(&debugPressure);

}
