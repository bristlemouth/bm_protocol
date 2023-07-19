#pragma once

#include "stm32_rtc.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef bool (*sensorSampleFn)();
typedef bool (*sensorInitFn)();
typedef bool (*sensorCheckFn)();

typedef struct {
  uint64_t uptime;
  RTCTimeAndDate_t rtcTime;
  uint16_t address;
  float voltage;
  float current;
} __attribute__((packed)) powerSample_t;

typedef struct {
  uint64_t uptime;
  RTCTimeAndDate_t rtcTime;
  float temperature;
  float humidity;
} __attribute__((packed)) humTempSample_t;

typedef struct {
  uint64_t uptime;
  RTCTimeAndDate_t rtcTime;
  float temperature;
  float pressure;
} __attribute__((packed)) pressureSample_t;

typedef struct {
  /// Initialization function
  sensorInitFn initFn;

  /// Sampling function (should push data to sensorhub!)
  sensorSampleFn sampleFn;

  /// (optional) Sensor check function
  sensorCheckFn checkFn;
} sensor_t;

typedef struct {
  uint16_t sensorCheckIntervalS;
  uint32_t sensorsPollIntervalMs;
} sensorConfig_t;

// Default configuration used in case sysConfig isn't loaded
#define SENSOR_DEFAULT_CONFIG { \
          .sensorCheckIntervalS=(30 * 60)}

void sensorSamplerInit(sensorConfig_t *config);
bool sensorSamplerAdd(sensor_t *sensor, const char *name);
bool sensorSamplerEnable(const char *name);
bool sensorSamplerDisable(const char *name);
bool sensorSamplerDisableChecks();
bool sensorSamplerEnableChecks();
uint32_t sensorSamplerGetSamplingPeriodMs(const char * name);
bool sensorSamplerChangeSamplingPeriodMs(const char * name, uint32_t new_period_ms);

#ifdef __cplusplus
}
#endif
