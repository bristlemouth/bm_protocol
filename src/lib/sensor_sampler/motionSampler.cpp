#include "motionSampler.h"
#include "lpm.h"
#include "task.h"
#include <array>
#include <string.h>

#ifndef MOTION_TASK_PRIORITY
#define MOTION_TASK_PRIORITY 3
#endif

MotionSampler::MotionSampler(SPIInterface_t *spi, IOPinHandle_t *cs_pin, IOPinHandle_t *int_pin)
    : m_lsm6dsv(this), m_lis2mdl(LIS2MDL_I2C_ADD) {
  m_ctx.spi = spi;
  m_ctx.cs = cs_pin;
  m_ctx.isr = int_pin;
}

/*!
 @brief Begin sensor communication handler

 @details This is invoked before communication to the sensor has started.
          Being a SPI device, the chip select line is driven low.
 */
void MotionSampler::begin(void) { IOWrite(m_ctx.cs, 0); }

/*!
 @brief Read from the sensor using SPI with DMA

 @details The LSM6DSV requires the most significant bit to be set to 1 in order
          to indicate a read from the device.

 @param reg Register to read
 @param buf Buffer size
 @param len Length of buffer
 @param arg unused

 @return BmOK on success
         BmErr on failure
 */
BmErr MotionSampler::read(uint8_t reg, uint8_t *buf, size_t len, void *arg) {
  (void)arg;
  reg = 1 << 7 | reg;
  spiTx(m_ctx.spi, NULL, 1, &reg, 100);
  return static_cast<BmErr>(spiRxNonblocking(m_ctx.spi, NULL, len, buf, 100));
}

/*!
 @brief Write to the sensor using SPI

 @param reg Register to write to
 @param buf Buffer size
 @param len Length of buffer
 @param arg unused

 @return BmOK on success
         BmErr on failure
 */
BmErr MotionSampler::write(uint8_t reg, const uint8_t *buf, size_t len, void *arg) {
  (void)arg;

  spiTx(m_ctx.spi, NULL, 1, &reg, 100);
  return static_cast<BmErr>(spiTx(m_ctx.spi, NULL, len, (uint8_t *)buf, 100));
}

/*!
 @brief End sensor communication handler

 @details This is invoked after communication to the sensor has finished.
          Being a SPI device, the chip select line is driven high.
 */
void MotionSampler::end(void) { IOWrite(m_ctx.cs, 1); }

/*!
 @brief Set configuration for motion sampler

 @details Must be invoked before running motion_sampler_add.

 @param cfg Configuration to set
 */
void MotionSampler::set_cfg(MotionSamplerConfig cfg) { m_ctx.cfg = cfg; }

/*!
 @brief Indicates if data is ready to be collected from the sensor

 @details If there is no sensor connected, this function will delay the
          timeout value.

 @param timeout_ms Timeout to wait for data to become available

 @return True if data is available
         False otherwise
 */
bool MotionSampler::data_ready(uint32_t timeout_ms) {
  BmErr err = m_lsm6dsv.reading_ready(timeout_ms);
  if (err == BmENODEV) {
    bm_delay(timeout_ms);
  }

  return err == BmOK;
}

/*!
 @brief Obtain imu data from the sensor

 @details If data is available, this function can be polled to clear out all
          of the available data reported from the sensor. i.e.:
            while (sensor.data_get(&reading) == BmOK) {
              // perform readings here
            }
          If magnetometer readings are not ready by the next time the
          IMU has a reading, the previous magnetometer reading will be
          used to interpolate the data.

 @param reading A single reading from the sensor

 @return BmOK on success
         BmErr on failure
 */
BmErr MotionSampler::data_get(IMUReading *reading) {
  LSM6DSV::LSM6DSVReading r;
  BmErr err = m_lsm6dsv.get_reading(&r);
  if (err == BmOK) {
    reading->ns = r.ns;
    reading->acc = {r.acc.x, r.acc.y, r.acc.z};
    reading->gyro = {r.gyro.x, r.gyro.y, r.gyro.z};

    // Use previous magnetometer reading if LSM is sampled at a higher rate
    LIS2MDL::LIS2MDLReading compass = m_compass_prev;
    if (m_compass_prev.ns <= reading->ns) {
      if (m_lis2mdl.get_reading(&compass) == BmOK) {
        m_compass_prev = compass;
      }
    }
    reading->mag = {compass.x, compass.y, compass.z};
  }
  return err;
}

static bool lsm6dsv_isr_handle(const void *pin, uint8_t value, void *args) {
  (void)pin;
  MotionSampler *sampler = static_cast<MotionSampler *>(args);

  if (value) {
    sampler->m_lsm6dsv.handle_interrupt();
  }

  return true;
}

/*!
 @brief Motion sensor task

 @details Will initialize/configure, and handle streaming data from the motion
          sensor.

 @param arg Sampler instance
 */
static void motion_task(void *arg) {
  MotionSampler *sampler = static_cast<MotionSampler *>(arg);
  LSM6DSV *lsm6dsv = &sampler->m_lsm6dsv;
  LIS2MDL *lis2mdl = &sampler->m_lis2mdl;

  if (lsm6dsv->init(&sampler->m_ctx.cfg) != BmOK) {
    vTaskDelete(NULL);
    return;
  }

  static constexpr uint8_t num_fifo_readings = 12;

  //TODO: add LISM compass to sensor hub
  std::array<LSM6DSV::LSM6DSVSensorHub, 1> sensor_hub = {lis2mdl->m_sensor_hub};

  lsm6dsv->start_stream(sensor_hub, num_fifo_readings);

  while (1) {
    lsm6dsv->stream_handle();
  }
}

/*!
 @brief Obtain the default configuration for the motion sensor

 @return Default motion sensor sampler configuration
 */
MotionSamplerConfig motion_sampler_get_default_config(void) {
  static const LSM6DSV::Cfg lsm6dsv_default_cfg = {
      .accelerometer =
          {
              .scale = LSM6DSV_2g,
              .mode = LSM6DSV_XL_HIGH_PERFORMANCE_MD,
          },
      .gyro =
          {
              .scale = LSM6DSV_125dps,
              .mode = LSM6DSV_GY_HIGH_PERFORMANCE_MD,
          },
      .sample_rate = LSM6DSV_ODR_AT_120Hz,
  };
  return lsm6dsv_default_cfg;
}

/*!
 @brief Add a motion sensor sampler

 @details Configures interrupt for the sampler ISR pin, raises the chip select
          line and creates an instance of a motion_task. Sensor must be
          configured beforehand.

 @param sampler Sampler instance to begin sampling

 @return BmOK on success
         BmErr on failure
 */
BmErr motion_sampler_add(MotionSampler *sampler) {
  if (!sampler) {
    return BmEINVAL;
  }

  IORegisterCallback(sampler->m_ctx.isr, lsm6dsv_isr_handle, sampler);
  IOWrite(sampler->m_ctx.cs, 1);

  static constexpr uint32_t stack_size = 1024;
  return bm_task_create(motion_task, "motion", stack_size, sampler, MOTION_TASK_PRIORITY, NULL);
}
