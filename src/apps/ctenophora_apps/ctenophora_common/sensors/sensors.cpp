#include <stdbool.h>
#include <stdint.h>
#include "bsp.h"
#include "sensors.h"
#include "FreeRTOS.h"

// Sensor driver includes
#include "ina232.h"

// Sampler initialization functions (so we don't need individual headers)
void powerSamplerInit(INA::INA232 **sensors); // implemented in src/lib/sensor_sampler/powerSampler.cpp
INA::INA232 debugIna1(&i2c1, I2C_INA_MAIN_ADDR);
INA::INA232 debugIna2(&i2c1, I2C_INA_PODL_ADDR);
INA::INA232 *debugIna[NUM_INA232_DEV] = {
  &debugIna1,
  &debugIna2,
};

void sensorsInit() {
  // Power monitor
  powerSamplerInit(debugIna);
}
