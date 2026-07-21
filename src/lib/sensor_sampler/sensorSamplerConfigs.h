#ifndef __SENSOR_SAMPLER_CONFIGS_H__
#define __SENSOR_SAMPLER_CONFIGS_H__

#include "configuration.h"
#include "sensorSampler.h"
#include "util.h"

#define DEFAULT_SENSORS_POLL_MS 10000

static inline bool get_sensor_poll_interval_ms(sensor_t *sensor) {
  return get_config_uint(BM_CFG_PARTITION_SYSTEM, "sensorsPollIntervalMs",
                         strlen("sensorsPollIntervalMs"), &sensor->intervalMs);
}

#endif
