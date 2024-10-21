#ifndef __HTU_SAMPLER_H
#define __HTU_SAMPLER_H

#include "stm32_rtc.h"

typedef struct {
  uint64_t uptime;
  RTCTimeAndDate_t rtcTime;
  float temperature;
  float humidity;
} __attribute__((packed)) humTempSample_t;

bool htuSamplerGetLatest(float &humidity, float &temperature);

#endif
