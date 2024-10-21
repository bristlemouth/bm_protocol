#include <stdint.h>
#include <string.h>
#include "debug.h"
#include "sensorSampler.h"
extern "C" {
#include "fnv.h"
}
#include "uptime.h"
#include "task.h"
#include "task_priorities.h"

// Flags not used for sensors
#define SENSOR_CHECK_SAMPLE_FLAG (1 << 30)
#define RESERVED_FLAG (1 << 31)

// Used to keep strncmp bounded, just in case
#define MAX_NAME_LEN 255

static LL_t ll = {NULL, NULL, NULL};
static uint8_t numSensors = 0;

static TaskHandle_t sensorSampleTaskHandle;

static sensorConfig_t *_config;

static TimerHandle_t sensorCheckTimer;

static void sensorSampleTask( void *parameters );
static void sensorTimerCallback(TimerHandle_t timer);

/*!
  Run the checkFn() on all sensors who have one. If the check fails,
  disable the sensor until the next check (and log the error).

  If the sensor was previously disabled but a check succeeds, it will
  be re-enabled.

  \param[in/out] *sensorItem - sensor item pointer to check
*/
static void checkSensors(sensorListItem_t *sensorItem) {

  // Check all sensors to see if they are due for a sample update
  // Only check if it was previously enabled (and if there's a check function!)
  if ((sensorItem->timer != NULL) && (sensorItem->sensor.checkFn != NULL)) {
    if (sensorItem->sensor.checkFn()) {
      // If the timer had been previously disabled, try to reinitialize
      // and start again
      if (!xTimerIsTimerActive(sensorItem->timer) && sensorItem->sensor.initFn()) {
        xTimerStart(sensorItem->timer, 10);
        printf("%llu | %s Re-enabled\n", uptimeGetMicroSeconds()/1000, sensorItem->name);
      }
    } else if (xTimerIsTimerActive(sensorItem->timer)) {
      xTimerStop(sensorItem->timer, 10);
      printf("%llu | %s Check Failed - Disabling\n", uptimeGetMicroSeconds()/1000, sensorItem->name);
    }
    // Otherwise, try to re-init if not initted
  } else if (_config->sensorsPollIntervalMs > 0 && sensorItem->timer == NULL) {
    printf("%llu | Attempting to re-init sensor %s\n", uptimeGetMicroSeconds()/1000, sensorItem->name);
    if (sensorItem->sensor.initFn()) {
      sensorItem->timer = xTimerCreate(
          sensorItem->name,
          pdMS_TO_TICKS(_config->sensorsPollIntervalMs),
          pdTRUE, // Enable auto-reload
          reinterpret_cast<void *>(&sensorItem->flag),
          sensorTimerCallback
      );
      configASSERT(sensorItem->timer != NULL);

      // Start sample timer
      configASSERT(xTimerStart(sensorItem->timer, 10));
    } else {
      printf("%llu | Error initializing %s\n", uptimeGetMicroSeconds() / 1000, sensorItem->name);
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
    2048,
    NULL,
    SENSOR_SAMPLER_TASK_PRIORITY,
    &sensorSampleTaskHandle);

  configASSERT(rval == pdTRUE);
}

/*!
  Deinitialize sensor sampling task and variables
*/
void sensorSamplerDeinit(void) {
  _config = NULL;
  ll = { NULL, NULL, NULL };
  vTaskDelete(sensorSampleTaskHandle);
  sensorCheckTimer = NULL;
  numSensors = 0;
}

static void sensorTimerCallback(TimerHandle_t timer) {
  xTaskNotify(sensorSampleTaskHandle, *(uint32_t*) pvTimerGetTimerID(timer), eSetBits);
}

/*!
  Add a new sensor for periodic sampling

  \param[in] *sensor - pointer to sensor_t struct to add
  \param[in] name - string identifier
  \return true
*/
bool sensorSamplerAdd(sensorNode_t *sensor, const char *name) {
  configASSERT(sensor != NULL);
  configASSERT(_config != NULL);
  configASSERT(numSensors < MAX_SENSORS);
  configASSERT(name != NULL);

  // Make sure the required functions are present
  configASSERT(sensor->list.sensor.initFn != NULL);
  configASSERT(sensor->list.sensor.sampleFn != NULL);
  // checkFn is optional

  // Set linked list node characteristics and add the linked list item
  sensor->list.name = name;
  sensor->list.flag = (1 << numSensors);
  sensor->node.id = fnv_32a_buf((void *) sensor->list.name,
                            strnlen(sensor->list.name, MAX_NAME_LEN),
                            0);
  sensor->node.data = &sensor->list;
  LLNodeAdd(&ll, &sensor->node);
  numSensors++;

  // Initialize sensor if needed
  if (_config->sensorsPollIntervalMs > 0){
    if(sensor->list.sensor.initFn()) {
      sensor->list.timer = xTimerCreate(
        sensor->list.name,
        pdMS_TO_TICKS(_config->sensorsPollIntervalMs),
        pdTRUE, // Enable auto-reload
        reinterpret_cast<void *>(&sensor->list.flag),
        sensorTimerCallback
      );
      configASSERT(sensor->list.timer != NULL);

      // Start sample timer
      configASSERT(xTimerStart(sensor->list.timer, 10));
    } else {
      printf("%llu | Error initializing %s\n",
              uptimeGetMicroSeconds()/1000, name);
    }
  } else {
    printf("%llu | %s Disabled\n", uptimeGetMicroSeconds()/1000, name);
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
  int ret = 0;
  sensorListItem_t *item = NULL;

  configASSERT(name);
  ret = LLGetElement(&ll,
                    fnv_32a_buf((void *) name,
                            strnlen(name, MAX_NAME_LEN),
                            0),
                    (void **) &item);
  if (ret == 0 && item && item->timer &&
      xTimerIsTimerActive(item->timer)) {
    configASSERT(xTimerStop(item->timer, 10));
    rval = true;
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

  int ret = 0;
  sensorListItem_t *item = NULL;

  configASSERT(name);
  ret = LLGetElement(&ll,
                    fnv_32a_buf((void *) name,
                            strnlen(name, MAX_NAME_LEN),
                            0),
                    (void **) &item);
  if (ret == 0 && item && item->timer &&
        !xTimerIsTimerActive(item->timer)) {
    configASSERT(xTimerStart(item->timer, 10));
    rval = true;
  }
  return rval;
}

/*!
  Disable periodic sensor checks. Could be useful during low power
  mode, otherwise sensor checks will re-enable previously disabled sensors

  \return true if successful, false otherwise
*/
bool sensorSamplerDisableChecks(void) {
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
bool sensorSamplerEnableChecks(void) {
  bool rval = false;

  if((sensorCheckTimer != NULL) && !xTimerIsTimerActive(sensorCheckTimer)) {
    configASSERT(xTimerStart(sensorCheckTimer, 10));
    rval = true;
  }

  return rval;
}

/*!
  Get the sampling period of a specified sensor

  \param[in] *name - name of the sensor node to get the sampling period
  \return period in ms if sensor node was found
*/
uint32_t sensorSamplerGetSamplingPeriodMs(const char * name) {
  TickType_t period_ticks = 0;
  int ret = 0;
  sensorListItem_t *item = NULL;

  configASSERT(name);
  ret = LLGetElement(&ll,
                    fnv_32a_buf((void *) name,
                            strnlen(name, MAX_NAME_LEN),
                            0),
                    (void **) &item);
  if (ret == 0 && item && item->timer) {
    period_ticks = xTimerGetPeriod(item->timer);
  }

  return (uint32_t)period_ticks * 1000 / configTICK_RATE_HZ;
}

/*!
  Change the sampling period of a specified sensor

  \param[in] *name - name of the sensor node to change the sampling period
  \param[in] new_period_ms - new sampling period for sensor node in ms
  \return true if node was found and it has an activated timer, false otherwise
*/
bool sensorSamplerChangeSamplingPeriodMs(const char * name, uint32_t new_period_ms) {
  bool rval = false;
  int ret = 0;
  sensorListItem_t *item = NULL;

  configASSERT(name);
  ret = LLGetElement(&ll,
                    fnv_32a_buf((void *) name,
                            strnlen(name, MAX_NAME_LEN),
                            0),
                    (void **) &item);
  // If timer is disabled, don't adjust period as that will start timer. Return false to indicate failure
  if (ret == 0 && item && item->timer &&
        xTimerIsTimerActive(item->timer)) {
    configASSERT(xTimerChangePeriod(item->timer, pdMS_TO_TICKS(new_period_ms), 10));
    rval = true;
  }

  return rval;
}

/*!
  Sensor sample task linked list traverse function callback,
  calls the sensor sampling function for the relevant sensor passed in to
  the traverse callback.
  Also periodically runs sensor checks (if enabled)

  \param[in] **data - data from linked list element (sensorListItem_t)
  \param[in] *arg - argument passed from traverse ll call (taskNotifyValue)
*/
static int taskTraverse(void *data, void *arg)
{
    int ret = EIO;
    sensorListItem_t *item = NULL;
    uint32_t notify = 0;
    if (data && arg)
    {
        ret = 0;
        item = (sensorListItem_t *) data;
        notify = *(uint32_t *) arg;
        if(item->flag & notify) {
           item->sensor.sampleFn();
        }
        if (notify & SENSOR_CHECK_SAMPLE_FLAG) {
            checkSensors(item);
        }
    }

    return ret;
}

/*!
  Setup in the sensor sample task.
  Responsible for creting the sensor check timer

*/
static void sensorSampleTaskSetup(void) {
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
    printf("Sensor Checks Disabled\n");
  }
}

/*!
  Loop in the sensor sample task
  Responsible for receiving notifications from created timers and handling accordingly.
*/
static void sensorSampleTaskLoop(void) {
  uint32_t taskNotifyValue = 0;
  BaseType_t res = xTaskNotifyWait(pdFALSE, UINT32_MAX, &taskNotifyValue, portMAX_DELAY);
  if(res != pdTRUE) {
    // Error
    printf("Error waiting for sensor task notification\n");
  } else {
    // Check all sensors to see if they are due for a sample update
    if (taskNotifyValue & SENSOR_CHECK_SAMPLE_FLAG) {
      printf("%llu | Running sensor checks.\n", uptimeGetMicroSeconds()/1000);
    }
    LLTraverse(&ll, taskTraverse, &taskNotifyValue);
  }

}

/*!
  Sensor sampling task. Waits for individual sensor timers to expire, then
  calls the sensor sampling function for the relevant sensor in linked list
  traverse function

  \param[in] *parameters - ignored
*/
static void sensorSampleTask( void *parameters ) {
  (void) parameters;

  sensorSampleTaskSetup();

  for (;;) {
    sensorSampleTaskLoop();
  }
}
