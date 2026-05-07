#include "sensors.h"
#include "FreeRTOS.h"
#include "bsp.h"
#include "lpm.h"
#include "motionSampler.h"
#include <stdbool.h>
#include <stdint.h>

#include "ina232.h"

// Sampler initialization functions (so we don't need individual headers)
void powerSamplerInit(
    INA::INA232 **sensors); // implemented in src/lib/sensor_sampler/powerSampler.cpp

static INA::INA232 debugIna1(&i2c1, I2C_INA_PODL_ADDR);
static INA::INA232 *debugIna[NUM_INA232_DEV] = {
    &debugIna1,
};

void sensorsInit() {
  // Wait for the 3V3 rail to stabilize before communicating with the mux
  vTaskDelay(pdMS_TO_TICKS(5));

  // Power monitor
  powerSamplerInit(debugIna);

  motionSensorAdd(&spi1, &BM_CS, &BM_INT);
}
