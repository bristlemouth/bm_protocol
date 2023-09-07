#include <stdbool.h>
#include <stdint.h>
#include "bsp.h"
#include "sensors.h"
#include "abstract_pressure_sensor.h"
#include "abstract_htu_sensor.h"
#include "FreeRTOS.h"

// Sensor driver includes
#include "ms5803.h"
#include "htu21d.h"
#include "ina232.h"
#include "bme280driver.h"
#include "tca9546a.h"

// Sampler initialization functions (so we don't need individual headers)
void powerSamplerInit(INA::INA232 **sensors); // implemented in src/lib/sensor_sampler/powerSampler.cpp

static TCA::TCA9546A bristlefinTCA(&i2c1, TCA9546A_ADDR, &I2C_MUX_RESET);
static MS5803 debugPressure(&i2c1, MS5803_ADDR);
static HTU21D debugHTU(&i2c1);
static Bme280 debugPHTU(&i2c1, Bme280::I2C_ADDR);
static INA::INA232 debugIna1(&i2c1, I2C_INA_MAIN_ADDR);
static INA::INA232 debugIna2(&i2c1, I2C_INA_PODL_ADDR);
static INA::INA232 *debugIna[NUM_INA232_DEV] = {
  &debugIna1,
  &debugIna2,
};
Bristlefin bristlefin(debugPressure, debugHTU, debugPHTU, bristlefinTCA, debugIna2);

void sensorsInit() {
  bristlefin.setGpioDefault();
  if(!bristlefin.sensorsInit()){
    printf("Failed to init bristlefin\n");
  }
  // Power monitor
  powerSamplerInit(debugIna); // There's technically one INA232 on the mote and one on Bristlefin,
  // keep the init functions out of Bristlefin now for convinience.
}
