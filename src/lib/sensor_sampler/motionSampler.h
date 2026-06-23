#ifndef __MOTION_SAMPLER_H__
#define __MOTION_SAMPLER_H__

#include "abstract_st_sensor.h"
#include "io.h"
#include "lsm6dsv.h"
#include "protected_spi.h"
#include "util.h"

typedef LSM6DSV::LSM6DSVReading MotionSensorReading;
typedef LSM6DSV::Cfg MotionSamplerConfig;

class MotionSampler : public SensorInterfaceBus {
public:
  MotionSampler(SPIInterface_t *spi, IOPinHandle_t *cs_pin, IOPinHandle_t *int_pin);

  void set_cfg(MotionSamplerConfig cfg);
  bool data_ready(uint32_t timeout_ms = 50);
  BmErr data_get(MotionSensorReading *reading);

  struct {
    SPIInterface_t *spi;
    IOPinHandle_t *cs;
    IOPinHandle_t *isr;
    LSM6DSV::Cfg cfg;
  } m_ctx;

  LSM6DSV m_lsm6dsv;

private:
  void begin(void) override;
  BmErr read(uint8_t reg, uint8_t *buf, size_t len, void *arg) override;
  BmErr write(uint8_t reg, const uint8_t *buf, size_t len, void *arg) override;
  void end(void) override;
};

MotionSamplerConfig motion_sampler_get_default_config(void);
BmErr motion_sampler_add(MotionSampler *sampler);

#endif
