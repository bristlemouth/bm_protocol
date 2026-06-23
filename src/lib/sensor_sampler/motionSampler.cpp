#include "motionSampler.h"
#include "lpm.h"
#include <array>
#include <string.h>

#ifndef MOTION_TASK_PRIORITY
#define MOTION_TASK_PRIORITY 3
#endif

MotionSampler::MotionSampler(SPIInterface_t *spi, IOPinHandle_t *cs_pin, IOPinHandle_t *int_pin)
    : m_lsm6dsv(this) {
  m_ctx.spi = spi;
  m_ctx.cs = cs_pin;
  m_ctx.isr = int_pin;
}

void MotionSampler::begin(void) { IOWrite(m_ctx.cs, 0); }

BmErr MotionSampler::read(uint8_t reg, uint8_t *buf, size_t len, void *arg) {
  (void)arg;
  reg = 1 << 7 | reg;
  spiTx(m_ctx.spi, NULL, 1, &reg, 100);
  return static_cast<BmErr>(spiRxNonblocking(m_ctx.spi, NULL, len, buf, 100));
}

BmErr MotionSampler::write(uint8_t reg, const uint8_t *buf, size_t len, void *arg) {
  (void)arg;

  spiTx(m_ctx.spi, NULL, 1, &reg, 100);
  return static_cast<BmErr>(spiTx(m_ctx.spi, NULL, len, (uint8_t *)buf, 100));
}

void MotionSampler::end(void) { IOWrite(m_ctx.cs, 1); }

void MotionSampler::set_cfg(MotionSamplerConfig cfg) { m_ctx.cfg = cfg; }

bool MotionSampler::data_ready(uint32_t timeout_ms) {
  BmErr err = m_lsm6dsv.reading_ready(timeout_ms);
  if (err == BmENODEV) {
    bm_delay(timeout_ms);
  }

  return err == BmOK;
}

BmErr MotionSampler::data_get(MotionSensorReading *reading) {

  return m_lsm6dsv.get_reading(reading);
}

static bool lsm6dsv_isr_handle(const void *pin, uint8_t value, void *args) {
  (void)pin;
  MotionSampler *sampler = static_cast<MotionSampler *>(args);

  if (value) {
    sampler->m_lsm6dsv.handle_interrupt();
  }

  return true;
}

static void motion_task(void *arg) {
  MotionSampler *sampler = static_cast<MotionSampler *>(arg);
  LSM6DSV *lsm6dsv = &sampler->m_lsm6dsv;

  if (lsm6dsv->init(&sampler->m_ctx.cfg) != BmOK) {
    return;
  }

  static constexpr uint8_t num_fifo_readings = 12;

  //TODO: add LISM compass to sensor hub
  std::array<LSM6DSV::LSM6DSVSensorHub, 0> sensor_hub = {};

  lsm6dsv->start_stream(sensor_hub, num_fifo_readings);

  while (1) {
    lsm6dsv->stream_handle();
  }
}

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

BmErr motion_sampler_add(MotionSampler *sampler) {
  if (!sampler) {
    return BmEINVAL;
  }

  IORegisterCallback(sampler->m_ctx.isr, lsm6dsv_isr_handle, sampler);

  // Start with chip select pin high
  IOWrite(sampler->m_ctx.cs, 1);

  static constexpr uint32_t stack_size = 1024;
  return bm_task_create(motion_task, "motion", stack_size, sampler, MOTION_TASK_PRIORITY, NULL);
}
