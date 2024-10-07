#pragma once

#include "ll.h"
#include "FreeRTOS.h"
#include "timers.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// We're using a 32-bit variable for sample flags (with two reserved)
#define MAX_SENSORS 30

/// Used to initialize sensor node
#define SENSOR_NODE_INIT(init, sample, check) \
    { .list = { \
          .sensor = { .initFn = init, .sampleFn = sample, .checkFn = check }, \
          .name = NULL, \
          .timer = NULL, \
          .flag = 0, }, \
      .node = {0, 0, 0, 0}, }

typedef bool (*sensorSampleFn)(void);
typedef bool (*sensorInitFn)(void);
typedef bool (*sensorCheckFn)(void);

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

typedef struct{
  /// Pointer to actual sensor struct
  sensor_t sensor;
  /// Sensor name/identifier
  const char *name;
  /// Sampling timer
  TimerHandle_t timer;
  /// Flag (single bit) used for task notification
  uint32_t flag;
} sensorListItem_t;

typedef struct {
    sensorListItem_t list;
    LLNode_t node;
} sensorNode_t;

// Default configuration used in case sysConfig isn't loaded
#define SENSOR_DEFAULT_CONFIG { \
          .sensorCheckIntervalS=(30 * 60), \
          .sensorsPollIntervalMs=(1000), }

void sensorSamplerInit(sensorConfig_t *config);
void sensorSamplerDeinit(void);
bool sensorSamplerAdd(sensorNode_t *sensor, const char *name);
bool sensorSamplerEnable(const char *name);
bool sensorSamplerDisable(const char *name);
bool sensorSamplerDisableChecks(void);
bool sensorSamplerEnableChecks(void);
uint32_t sensorSamplerGetSamplingPeriodMs(const char * name);
bool sensorSamplerChangeSamplingPeriodMs(const char * name, uint32_t new_period_ms);

#ifdef __cplusplus
}
#endif
