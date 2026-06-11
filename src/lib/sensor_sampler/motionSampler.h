#ifndef __MOTION_SAMPLER_H__
#define __MOTION_SAMPLER_H__

#include "io.h"
#include "lsm6dsv.h"
#include "protected_spi.h"
#include "util.h"

typedef LSM6DSV::LSM6DSVReading MotionSensorReading;
typedef LSM6DSV::Cfg MotionSamplerConfig;

MotionSamplerConfig motionSensorGetDefaultConfig(void);
BmErr motionSensorAdd(MotionSamplerConfig cfg, SPIInterface_t *spi, IOPinHandle_t *cs_pin,
                      IOPinHandle_t *int_pin);
BmErr motionSensorDataReady(uint32_t timeout_ms = 50);
BmErr motionSensorGet(MotionSensorReading *reading);

#endif
