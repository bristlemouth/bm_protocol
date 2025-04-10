#ifndef __POWER_SAMPLER_H__
#define __POWER_SAMPLER_H__

#include <stdint.h>

#define POWER_SAMPLER_NAME "PWR"

bool powerSamplerGetLatest(uint8_t power_monitor_address, float &voltage, float &current);

#endif
