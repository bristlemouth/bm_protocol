#ifndef __PRESSURE_SAMPLER_H
#define __PRESSURE_SAMPLER_H

#include "stm32_rtc.h"

typedef struct {
  uint64_t uptime;
  RTCTimeAndDate_t rtcTime;
  float temperature;
  float pressure;
} __attribute__((packed)) pressureSample_t;

bool pressureSamplerGetLatest(float &pressure, float &temperature);

#endif
