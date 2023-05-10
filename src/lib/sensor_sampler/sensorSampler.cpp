#include <stdint.h>
#include <string.h>
#include "FreeRTOS.h"
#include "debug.h"
#include "sensorSampler.h"
#include "task.h"
#include "task_priorities.h"
#include "timers.h"

// Flags not used for sensors
#define SENSOR_CHECK_SAMPLE_FLAG (1 << 30)
#define RESERVED_FLAG (1 << 31)

// We're using a 32-bit variable for sample flags (with two reserved)
#define MAX_SENSORS 30

// Used to keep strncmp bounded, just in case
#define MAX_NAME_LEN 255

typedef struct{
  /// Pointer to actual sensor struct
  sensor_t *sensor;

  /// Sensor name/identifier
  const char *name;

  /// Sampling timer
  TimerHandle_t timer;

  /// Flag (single bit) used for task notification
  uint32_t flag;
} sensorListItem_t;

// TODO - use linked list instead of pre-allocating
static sensorListItem_t *sensorList[MAX_SENSORS];
static uint8_t numSensors = 0;

static TaskHandle_t sensorSampleTaskHandle;

static sensorConfig_t *_config;

static TimerHandle_t sensorCheckTimer;

static void sensorSampleTask( void *parameters );

/*!
  Run the checkFn() on all sensors who have one. If the check fails,
  disable the sensor until the next check (and log the error).

  If the sensor was previously disabled but a check succeeds, it will
  be re-enabled.
*/
void checkSensors() {
  // logPrint(SYSLog, LOG_LEVEL_DEBUG, "Running sensor checks.\n");
  printf("Running sensor checks.\n");

  // Check all sensors to see if they are due for a sample update
  for(uint32_t sensorIdx = 0; sensorIdx < numSensors; sensorIdx++) {
    sensorListItem_t *sensorItem = sensorList[sensorIdx];

    // Only check if it was previously enabled (and if there's a check function!)
    if((sensorItem->timer != NULL) && (sensorItem->sensor->checkFn != NULL)) {
      if(sensorItem->sensor->checkFn()) {
        // If the timer had been previously disabled, try to reinitialize
        // and start again
        if (!xTimerIsTimerActive(sensorItem->timer) && sensorItem->sensor->initFn()) {
          xTimerStart(sensorItem->timer, 10);
          // logPrint(SYSLog, LOG_LEVEL_INFO, "%s Re-enabled\n", sensorItem->name);
          printf("%s Re-enabled\n", sensorItem->name);
        }
      } else if(xTimerIsTimerActive(sensorItem->timer)) {
        xTimerStop(sensorItem->timer, 10);
        // logPrint(SYSLog, LOG_LEVEL_ERROR, "%s Check Failed - Disabling\n", sensorItem->name);
        printf("%s Check Failed - Disabling\n", sensorItem->name);
      }
    }
  }
}

/*!
  Initialize sensor sampling task

  \param[in] *config - pointer to sensor sampling configuration
*/
void sensorSamplerInit(sensorConfig_t *config) {

  configASSERT(config != NULL);

  _config = config;

	BaseType_t rval = xTaskCreate(
    sensorSampleTask,
    "sensorSample",
    configMINIMAL_STACK_SIZE * 2,
    NULL,
    SENSOR_SAMPLER_TASK_PRIORITY,
    &sensorSampleTaskHandle);

  configASSERT(rval == pdTRUE);
}

static void sensorTimerCallback(TimerHandle_t timer) {
  xTaskNotify(sensorSampleTaskHandle, (uint32_t)pvTimerGetTimerID(timer), eSetBits);
}

/*!
  Add a new sensor for periodic sampling

  \param[in] *sensor - pointer to sensor_t struct to add
  \param[in] name - string identifier
  \return true
*/
bool sensorSamplerAdd(sensor_t *sensor, const char *name) {
  configASSERT(sensor != NULL);
  configASSERT(numSensors < MAX_SENSORS);
  configASSERT(name != NULL);

  // Make sure the required functions are present
  configASSERT(sensor->initFn != NULL);
  configASSERT(sensor->sampleFn != NULL);
  // checkFn is optional

  sensorList[numSensors] = static_cast<sensorListItem_t *>(pvPortMalloc(sizeof(sensorListItem_t)));
  configASSERT(sensorList[numSensors] != NULL);

  memset(sensorList[numSensors], 0, sizeof(sensorListItem_t));

  sensorListItem_t *sensorItem = sensorList[numSensors];
  sensorItem->sensor = sensor;
  sensorItem->name = name;
  sensorItem->flag = (1 << numSensors);
  numSensors++;

  // Initialize sensor if needed
  if (sensorItem->sensor->intervalMs > 0){
    if(sensorItem->sensor->initFn()) {
      sensorItem->timer = xTimerCreate(
        name,
        pdMS_TO_TICKS(sensorItem->sensor->intervalMs),
        pdTRUE, // Enable auto-reload
        reinterpret_cast<void *>(sensorItem->flag),
        sensorTimerCallback
      );
      configASSERT(sensorItem->timer != NULL);

      // Start sample timer
      configASSERT(xTimerStart(sensorItem->timer, 10));
    } else {
      // logPrint(SYSLog, LOG_LEVEL_INFO, "Error initializing %s\n", name);
      printf("Error initializing %s\n", name);
    }
  } else {
    // logPrint(SYSLog, LOG_LEVEL_INFO, "%s Disabled\n", name);
    printf("%s Disabled\n", name);
  }

  return true;
}

/*!
  Disable a sensor

  \param[in] name - string identifier
  \return true if disabled successfully, false otherwise
*/
bool sensorSamplerDisable(const char *name) {
  bool rval = false;

  for(uint32_t sensorIdx = 0; sensorIdx < numSensors; sensorIdx++) {
    if(strncmp(sensorList[sensorIdx]->name, name, MAX_NAME_LEN) == 0) {
      if(sensorList[sensorIdx]->timer && xTimerIsTimerActive(sensorList[sensorIdx]->timer)) {
        configASSERT(xTimerStop(sensorList[sensorIdx]->timer, 10));
      }
      rval = true;
      break;
    }
  }

  return rval;
}

/*!
  Re-enable a sensor

  \param[in] name - string identifier
  \return true if enabled successfully, false otherwise
*/
bool sensorSamplerEnable(const char *name) {
  bool rval = false;

  for(uint32_t sensorIdx = 0; sensorIdx < numSensors; sensorIdx++) {
    if(strncmp(sensorList[sensorIdx]->name, name, MAX_NAME_LEN) == 0) {
      if(sensorList[sensorIdx]->timer != NULL &&
          !xTimerIsTimerActive(sensorList[sensorIdx]->timer)) {
        configASSERT(xTimerStart(sensorList[sensorIdx]->timer, 10));
        rval = true;
      }
      break;
    }
  }

  return rval;
}

/*!
  Disable periodic sensor checks. Could be useful during low power
  mode, otherwise sensor checks will re-enable previously disabled sensors

  \return true if successful, false otherwise
*/
bool sensorSamplerDisableChecks() {
  bool rval = false;
  if((sensorCheckTimer != NULL) && xTimerIsTimerActive(sensorCheckTimer)) {
    configASSERT(xTimerStop(sensorCheckTimer, 10));
    rval = true;
  }

  return rval;
}

/*!
  Re-enable sensor checks (if previously disabled).

  \return true if successful, false otherwise
*/
bool sensorSamplerEnableChecks() {
  bool rval = false;

  if((sensorCheckTimer != NULL) && !xTimerIsTimerActive(sensorCheckTimer)) {
    configASSERT(xTimerStart(sensorCheckTimer, 10));
    rval = true;
  }

  return rval;
}

uint32_t sensorSamplerGetSamplingPeriodMs(const char * name) {
  TickType_t period_ticks = 0;

  for(uint32_t sensorIdx = 0; sensorIdx < numSensors; sensorIdx++) {
    if(strncmp(sensorList[sensorIdx]->name, name, MAX_NAME_LEN) == 0) {
      if(sensorList[sensorIdx]->timer != NULL) {
        period_ticks = xTimerGetPeriod(sensorList[sensorIdx]->timer);
      }
      break;
    }
  }

  return (uint32_t)period_ticks * 1000 / configTICK_RATE_HZ;
}

bool sensorSamplerChangeSamplingPeriodMs(const char * name, uint32_t new_period_ms) {
  bool rval = false;

  for(uint32_t sensorIdx = 0; sensorIdx < numSensors; sensorIdx++) {
    if(strncmp(sensorList[sensorIdx]->name, name, MAX_NAME_LEN) == 0) {
      // If timer is disabled, don't adjust period as that will start timer. Return false to indicate failure
      if(sensorList[sensorIdx]->timer != NULL && xTimerIsTimerActive(sensorList[sensorIdx]->timer)) {
        configASSERT(xTimerChangePeriod(sensorList[sensorIdx]->timer, pdMS_TO_TICKS(new_period_ms), 10));
        rval = true;
      }
      break;
    }
  }

  return rval;
}

/*!
  Sensor sampling task. Waits for individual sensor timers to expire, then
  calls the sensor sampling function for the relevant sensor.

  Also periodically runs sensor checks (if enabled)

  \param[in] *parameters - ignored
*/
static void sensorSampleTask( void *parameters ) {
  (void) parameters;

  if(_config->sensorCheckIntervalS) {
    sensorCheckTimer = xTimerCreate(
      "sensorCheck",
      pdMS_TO_TICKS((uint32_t)_config->sensorCheckIntervalS * 1000),
      pdTRUE, // Enable auto-reload
      reinterpret_cast<void *>(SENSOR_CHECK_SAMPLE_FLAG),
      sensorTimerCallback
    );
    configASSERT(sensorCheckTimer != NULL);
    xTimerStart(sensorCheckTimer, 10);
  } else {
    // logPrint(SYSLog, LOG_LEVEL_INFO, "Sensor Checks Disabled\n");
    printf("Sensor Checks Disabled\n");
  }

  for (;;) {
    uint32_t taskNotifyValue = 0;
    BaseType_t res = xTaskNotifyWait(pdFALSE, UINT32_MAX, &taskNotifyValue, portMAX_DELAY);
    if(res != pdTRUE) {
      // Error
      // logPrint(SYSLog, LOG_LEVEL_ERROR, "Error waiting for sensor task notification\n");
      printf("Error waiting for sensor task notification\n");
      continue;
    }

    // Check all sensors to see if they are due for a sample update
    for(uint32_t sensorIdx = 0; sensorIdx < numSensors; sensorIdx++) {
      if(sensorList[sensorIdx]->flag & taskNotifyValue) {
        sensorList[sensorIdx]->sensor->sampleFn();
      }
    }

    if (taskNotifyValue & SENSOR_CHECK_SAMPLE_FLAG) {
      checkSensors();
    }
  }
}
