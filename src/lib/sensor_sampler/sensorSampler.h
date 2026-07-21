#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef bool (*sensorSampleFn)(void);
typedef bool (*sensorInitFn)(void);
typedef bool (*sensorCheckFn)(void);

typedef struct {
  uint32_t intervalMs;     // Sample interval in milliseconds
  sensorInitFn initFn;     // Initialization function
  sensorSampleFn sampleFn; // Sampling function (should push data to sensorhub!)
  sensorCheckFn checkFn;   // Sensor check function (optional)
} sensor_t;

typedef struct {
  uint16_t sensorCheckIntervalS; // How often sensor checks shall be performed,
                                 // if zero, sensor checks disabled
} sensorConfig_t;

// Default configuration used in case sysConfig isn't loaded
#define SENSOR_DEFAULT_CONFIG {.sensorCheckIntervalS = (30 * 60)}

void sensorSamplerInit(sensorConfig_t *cfg);
bool sensorSamplerAdd(sensor_t *sensor, const char *name);
bool sensorSamplerEnable(const char *name);
bool sensorSamplerDisable(const char *name);
bool sensorSamplerDisableChecks();
bool sensorSamplerEnableChecks();
uint32_t sensorSamplerGetSamplingIntervalMs(const char *name);
bool sensorSamplerChangeSamplingIntervalMs(const char *name, uint32_t new_period_ms);

#ifdef __cplusplus
}
#endif
