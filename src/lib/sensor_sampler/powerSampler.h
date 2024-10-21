#ifndef __POWER_SAMPLER_H
#define __POWER_SAMPLER_H

#include "stm32_rtc.h"

typedef struct {
  uint64_t uptime;
  RTCTimeAndDate_t rtcTime;
  uint16_t address;
  float voltage;
  float current;
} __attribute__((packed)) powerSample_t;

bool powerSamplerGetLatest(uint8_t power_monitor_address, float &voltage, float &current);

#endif
