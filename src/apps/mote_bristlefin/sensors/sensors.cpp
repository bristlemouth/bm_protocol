#include <stdbool.h>
#include <stdint.h>
#include "bsp.h"
#include "sensors.h"
#include "abstract_pressure_sensor.h"
#include "abstract_htu_sensor.h"

// Sensor driver includes
#include "bme280driver.h"
#include "ms5803.h"
#include "htu21d.h"
#include "ina232.h"

// Sampler initialization functions (so we don't need individual headers)
void powerSamplerInit(INA::INA232 **sensors);
void pressureSamplerInit(AbstractPressureSensor *sensor);
void htuSamplerInit(AbstractHtu *sensor);
Bme280 debugPHTU(&i2c1, Bme280::I2C_ADDR);

MS5803 debugPressure(&i2c1, MS5803_ADDR);
HTU21D debugHTU(&i2c1);
INA::INA232 debugIna1(&i2c1, I2C_INA_MAIN_ADDR);
INA::INA232 debugIna2(&i2c1, I2C_INA_PODL_ADDR);
INA::INA232 *debugIna[NUM_INA232_DEV] = {
  &debugIna1,
  &debugIna2,
};

void sensorsInit() {
  static constexpr uint8_t PROBE_MAX_TRIES = 5;
  // Power monitor
  powerSamplerInit(debugIna);

  uint8_t try_count = 0;
  I2CResponse_t ret = debugPHTU.probe();
  while(ret != I2C_OK && try_count++ < PROBE_MAX_TRIES){
    ret = debugPHTU.probe();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  if(ret == I2C_OK) {
    // Initialize temperature/humidity
    htuSamplerInit(&debugPHTU);

    // Initialize Barometer sampling
    pressureSamplerInit(&debugPHTU);
  } else {
    // Initialize temperature/humidity
    htuSamplerInit(&debugHTU);

    // Initialize Barometer sampling
    pressureSamplerInit(&debugPressure);
  }

}
