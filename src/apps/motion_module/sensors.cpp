#include "sensors.h"
#include "bm_config.h"
#include "bsp.h"
#include "configuration.h"
#include "ina232.h"
#include "kellerSampler.h"
#include "lpm.h"
#include "spotter.h"
#include <stdbool.h>
#include <stdint.h>
#if defined(IMU_BNO085)
#include "bno085Sampler.h"
#else
#include "motionSampler.h"
#endif

static void keller_sample_cb(float mbar, float temp) {
  spotter_log(0, "pressure_raw.log", USE_TIMESTAMP, "pressure: %" PRIu64 ",%f,%f\n",
              uptimeGetMicroSeconds(), mbar, temp);
}

// Sampler initialization functions (so we don't need individual headers)
void powerSamplerInit(
    INA::INA232 **sensors); // implemented in src/lib/sensor_sampler/powerSampler.cpp

static INA::INA232 debugIna1(&i2c1, I2C_INA_PODL_ADDR);
static INA::INA232 *debugIna[NUM_INA232_DEV] = {
    &debugIna1,
};

#if defined(IMU_BNO085)
static Bno085Sampler imu(&spi1, &BM_CS, &BM_INT, &I2C_MUX_RESET, &GPIO1, &GPIO2);
#else
static MotionSampler imu(&spi1, &BM_CS, &BM_INT);
#endif

static KellerSampler keller(&i2c1, &IOEXP_INT, keller_sample_cb);

void sensorsInit(void) {
  // Wait for the 3V3 rail to stabilize before communicating with the mux
  vTaskDelay(pdMS_TO_TICKS(5));

  // Power monitor
  powerSamplerInit(debugIna);

#if defined(IMU_BNO085)
  Bno085SamplerConfig cfg = bno085_sampler_get_default_config();
  imu.set_cfg(cfg);
  configASSERT(bno085_sampler_add(&imu) == BmOK);
#else
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
  imu.set_cfg(cfg);
  configASSERT(motion_sampler_add(&imu) == BmOK);
#endif
  keller_sampler_add(&keller);
}

void sensorsHandle(void) {
  if (imu.data_ready()) {
    IMUReading reading = {};
    while (imu.data_get(&reading) == BmOK) {
      spotter_log(0, "imu_raw.log", USE_TIMESTAMP,
                  "imu: %" PRIu64 ",%f,%f,%f,%f,%f,%f,%f,%f,%f\n", reading.ns, reading.acc.x,
                  reading.acc.y, reading.acc.z, reading.gyro.x, reading.gyro.y, reading.gyro.z,
                  reading.mag.x, reading.mag.y, reading.mag.z);
    }
  }
}
