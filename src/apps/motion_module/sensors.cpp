#include "sensors.h"
#include "bm_config.h"
#include "bsp.h"
#include "configuration.h"
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
static MotionSampler motion(&spi1, &BM_CS, &BM_INT);

void sensorsInit(void) {
  // Wait for the 3V3 rail to stabilize before communicating with the mux
  vTaskDelay(pdMS_TO_TICKS(5));

  // Power monitor
  powerSamplerInit(debugIna);

  // Obtain configs for motion sensing module
  MotionSamplerConfig cfg = motion_sampler_get_default_config();
  uint32_t acc_scale = 0, gyro_scale = 0, sample_rate = 0;
  if (get_config_uint(BM_CFG_PARTITION_SYSTEM, "accScale", strlen("accScale"), &acc_scale)) {
    cfg.accelerometer.scale = static_cast<lsm6dsv_xl_full_scale_t>(acc_scale);
  }
  if (get_config_uint(BM_CFG_PARTITION_SYSTEM, "gyroScale", strlen("gyroScale"), &gyro_scale)) {
    cfg.gyro.scale = static_cast<lsm6dsv_gy_full_scale_t>(gyro_scale);
  }
  if (get_config_uint(BM_CFG_PARTITION_SYSTEM, "sampleRate", strlen("sampleRate"),
                      &sample_rate)) {
    cfg.sample_rate = static_cast<lsm6dsv_data_rate_t>(sample_rate);
  }
  motion.set_cfg(cfg);
  motion_sampler_add(&motion);
}

void sensorsHandle(void) {
  if (motion.data_ready()) {
    IMUReading imu = {};
    CompassReading compass = {};
    while (motion.data_get(&imu) == BmOK) {
      //bm_debug("imu: %" PRIu64 ",%f,%f,%f,%f,%f,%f\n", imu.ns, imu.acc.x, imu.acc.y, imu.acc.z,
      //         imu.gyro.x, imu.gyro.y, imu.gyro.z);
    }
    while (motion.data_get(&compass) == BmOK) {
      bm_debug("compass: %" PRIu64 ",%f,%f,%f\n", compass.ns, compass.x, compass.y, compass.z);
    }
  }
}
