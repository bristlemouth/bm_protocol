#include <stdbool.h>
#include <stdint.h>
#include "bsp.h"
#include "sensors.h"

// Sensor driver includes
#include "ms5803.h"

// Sampler initialization functions (so we don't need individual headers)
// void powerSamplerInit();
void pressureSamplerInit(MS5803 *sensor);
// void htuSampler();

MS5803 debugPressure(&i2c1, MS5803_ADDR);

void sensorsInit() {

  // TODO - implement this
  // Power monitor
  // powerSamplerInit();

  // TODO - implement this
  // Initialize temperature/humidity
  // htuSampler();

  // Initialize Barometer sampling
  pressureSamplerInit(&debugPressure);

}
