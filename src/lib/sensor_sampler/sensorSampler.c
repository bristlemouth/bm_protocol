#include "sensorSampler.h"
#include "FreeRTOS.h"
#include "bm_config.h"
#include "bm_os.h"
#include "crc.h"
#include "ll.h"
#include "semphr.h"
#include "stm32u5xx.h"
#include "task.h"
#include "task_priorities.h"
#include "util.h"
#include <stdint.h>
#include <string.h>

#define CHECK_FLAG (1 << 0)   // Has the sensor passed sensor check
#define ENABLED_FLAG (1 << 1) // Is the sensor enabled
#define INIT_FLAG (1 << 2)    // Has the sensor been initialized

typedef struct {
  TaskHandle_t handle; // Task handle for the sensor's task
  sensor_t *sensor;    // Pointer to sensor struct
  const char *name;    // Sensor name/identifier
  uint8_t flags;       // Sensor list flags
} SensorListItem;

struct SensorSampleCtx {
  LL sensor_list;          // List to hold sensor objects
  sensorConfig_t *cfg;     // Sensor sampler configuration
  TaskHandle_t check_task; // Handle for sensor check task
  SemaphoreHandle_t lock;
};

static struct SensorSampleCtx ctx = {0};

/*!
 @brief Calculate Sensor ID For sensor_list

 @details This calculates the CRC used for the sensor list based on
          the name of the sensor used during sensorSamplerAdd

 @param name Name of the sensor

 @return CRC calculation of the sensor name
 */
static uint32_t calc_sensor_id(const char *name) {
  return crc32_ieee((const void *)name, strlen(name));
}

/*!
 @brief Check Sensor And Determine If It Should Be Enabled Or Disabled

 @details This is invoked by a ll_traverse call. Will call the checkFn
          configured for the sensor. If the sensor previously failed a check
          and then succeeds a subsequent check, sampling on the sensor will be
          enabled. If a sensor fails a check, sampling on the sensor will be
          disabled.

 @param data points to a SensorListItem of the sensor to be checked
 @param arg Unused

 @return BmOK always
 */
static BmErr check_sensors(void *data, void *arg) {
  (void)arg;

  if (!data) {
    return BmOK;
  }

  SensorListItem *sensor_item = (SensorListItem *)data;

  // Ensure that the sensor has been initialized before checking it
  uint8_t init_flag = READ_BIT(sensor_item->flags, INIT_FLAG);
  if (init_flag && sensor_item->sensor->checkFn) {
    uint8_t check_flag = READ_BIT(sensor_item->flags, CHECK_FLAG);

    // Check sensor and clear check flag if check function fails
    if (sensor_item->sensor->checkFn()) {
      // If previously flag cleared and sensor can be initialized, set check flag
      if (!check_flag && sensor_item->sensor->initFn()) {
        SET_BIT(sensor_item->flags, CHECK_FLAG);
        xTaskNotifyGive(sensor_item->handle);
        bm_debug("%s Re-enabled\n", sensor_item->name);
      }
    } else if (check_flag) {
      CLEAR_BIT(sensor_item->flags, CHECK_FLAG);
      xTaskNotifyGive(sensor_item->handle);
      bm_debug("%s Check Failed - Disabling\n", sensor_item->name);
    }
  }

  return BmOK;
}

/*!
 @brief Samples Sensors Added To The Sensor Sampler

 @details This task is created for each individual sensor on the system.
          Sensors are sampled at their configured intervalMs rate. The
          task waits for a notification and if none is received will sample
          the sensor. The notification is used to handle changes in the
          sensor's flags or a change in its intervalMs configuration parameter.
          If a sensor cannot be initialized this deletes the sensor and task.

          Key responsibilities:
            - Sample sensor at intervalMs by invoking the sensor's sampleFn
            - Handle changes to the sensor's configurations and flags
            - Calculate the wait time before next sample based on how long
              previous sample takes
            - Handle overflows to wait time if a sensor took longer to read
              than configured intervalMs
            - Failed initialization removes sensor from sampling and check task

 @param parameters The SensorListItem for the sensor being sampled
 */
static void sensor_sample_task(void *parameters) {
  SensorListItem *sensor_item = (SensorListItem *)parameters;
  bool can_sample = false;
  uint32_t sensor_sample_interval_tick = pdMS_TO_TICKS(sensor_item->sensor->intervalMs);
  uint32_t time_to_sleep_tick = sensor_sample_interval_tick;

  if (sensor_item->sensor->initFn()) {
    SET_BIT(sensor_item->flags, INIT_FLAG);
    for (;;) {
      // Account for an overflow if sensor took longer than expected to read
      if (time_to_sleep_tick > sensor_sample_interval_tick) {
        time_to_sleep_tick = sensor_sample_interval_tick;
      }

      // Determine if sensor can sample based on flags and sample
      can_sample = READ_BIT(sensor_item->flags, CHECK_FLAG) &&
                   READ_BIT(sensor_item->flags, ENABLED_FLAG);

      // Wait until sensor can sample, if notification given this indicates
      // the sample rate has been changed, or a flag has been set/cleared for
      // the can_sample variable
      if (!ulTaskNotifyTake(pdTRUE, time_to_sleep_tick) && can_sample) {
        uint32_t begin_sample_time_tick = xTaskGetTickCount();

        sensor_item->sensor->sampleFn();

        // Ensures that task sleeps the configured amount of time
        time_to_sleep_tick =
            sensor_sample_interval_tick - (xTaskGetTickCount() - begin_sample_time_tick);
      } else {
        // Set to new interval
        sensor_sample_interval_tick = pdMS_TO_TICKS(sensor_item->sensor->intervalMs);
        time_to_sleep_tick = sensor_sample_interval_tick;
      }
    }
  } else {
    // If initialization fails, delete from sensor_list and the task
    bm_debug("Error initializing %s\n", sensor_item->name);
    xSemaphoreTake(ctx.lock, portMAX_DELAY);
    ll_remove(&ctx.sensor_list, calc_sensor_id(sensor_item->name));
    xSemaphoreGive(ctx.lock);
    vTaskDelete(NULL);
  }
}

/*!
 @brief Handles Checking Sensors To See If They Are Operational

 @details Checks sensors at sensor sampler's configured sensorCheckIntervalS.

          Key responsibilities:
            - Iterate through sensor list and invoke check_sensors function

 @param parameters pointer to sensor sampler configured sensorCheckIntervalS
 */
static void sensor_check_task(void *parameters) {
  uint32_t check_interval_ms = s_to_ms(*(uint32_t *)parameters);
  LL sensor_list = {0};

  for (;;) {
    // Wait until sensor check, then iterate through sensor list and check all sensors
    bm_delay(check_interval_ms);
    // Copy sensor list here as it might change within operation
    xSemaphoreTake(ctx.lock, portMAX_DELAY);
    sensor_list = ctx.sensor_list;
    xSemaphoreGive(ctx.lock);
    bm_debug("Running sensor checks.\n");
    ll_traverse(&sensor_list, check_sensors, NULL);
  }
}

/*!
 @brief Start The sensor_check_task

 @details This is called during initialization or if the sensor check is 
          re-enabled after being disabled. Will only start the task if
          sensorCheckIntervalS configuration value is non-zero.
 
 @returns true if task was started, false otherwise
 */
static inline bool start_check_task(void) {
  bool ret = false;

  if (!ctx.cfg->sensorCheckIntervalS) {
    bm_debug("Sensor Checks Disabled\n");
  } else {
    xSemaphoreTake(ctx.lock, portMAX_DELAY);
    BmErr err = bm_task_create(sensor_check_task, "sensor_check", configMINIMAL_STACK_SIZE * 2,
                               &ctx.cfg->sensorCheckIntervalS, SENSOR_SAMPLER_TASK_PRIORITY,
                               &ctx.check_task);
    xSemaphoreGive(ctx.lock);
    ret = err == BmOK;
  }

  return ret;
}

/*!
 @brief Initialize Sensor Sampler

 @param config Pointer to sensor sampling configuration
 */
void sensorSamplerInit(sensorConfig_t *cfg) {

  configASSERT(cfg != NULL);

  ctx.lock = xSemaphoreCreateMutex();
  configASSERT(ctx.lock);
  ctx.cfg = cfg;

  start_check_task();
}

/*!
 @brief Add a new sensor for periodic sampling

 @details Will start a task having the name of the sensor and adds the sensor
          to the sensor_list. By default the sensor is enabled (ENABLED_FLAG)
          and the check flag (CHECK_FLAG) is also set to ensure the sensor
          can begin sampling as soon as it is ready.
          There is no theoretical limit to the number of sensor's added to the
          system, but physical RAM constraints limit the number of sensors.
          Each sensor (task and data) allocates ~1k memory to the heap.

 @param sensor Pointer to sensor_t struct to add, must have a persistent
               lifetime 
 @param name Sensor string identifier

 @return true if sensor can be added or if intervalMs is 0,
         false on failure
*/
bool sensorSamplerAdd(sensor_t *sensor, const char *name) {
  configASSERT(sensor != NULL);
  configASSERT(name != NULL);

  // Make sure the required functions are present
  configASSERT(sensor->initFn != NULL);
  configASSERT(sensor->sampleFn != NULL);
  // checkFn is optional

  // If interval is 0, the sensor is disabled
  if (sensor->intervalMs == 0) {
    bm_debug("%s Disabled\n", name);
    return true;
  }

  SensorListItem sensor_item = {
      .handle = NULL,
      .sensor = sensor,
      .name = name,
      .flags = CHECK_FLAG | ENABLED_FLAG,
  };

  // Attempt to add item to sensor list
  LLItem *item = NULL;
  BmErr err = BmOK;
  item = ll_create_item(item, &sensor_item, sizeof(SensorListItem),
                        calc_sensor_id(sensor_item.name));
  xSemaphoreTake(ctx.lock, portMAX_DELAY);
  err = ll_item_add(&ctx.sensor_list, item);
  xSemaphoreGive(ctx.lock);
  if (err != BmOK) {
    vPortFree(item);
    return false;
  }

  // Create task to handle new sensor
  SensorListItem *p_ll_item = (SensorListItem *)item->data;
  err = bm_task_create(sensor_sample_task, p_ll_item->name, configMINIMAL_STACK_SIZE * 2,
                       p_ll_item, SENSOR_SAMPLER_TASK_PRIORITY, &p_ll_item->handle);
  if (err != pdTRUE) {
    vPortFree(item);
    return false;
  }

  return true;
}

/*!
 @brief Disable A Sensor

 @details Clears the ENABLED_FLAG, blocking the sensor from sampling in it's
          task. Notifies the task to indicate that the sampling has been
          affected.

 @param name Sensor string identifier

 @return true if disabled successfully, false otherwise
*/
bool sensorSamplerDisable(const char *name) {
  bool ret = false;
  SensorListItem *sensor_item = NULL;

  if (!name) {
    return ret;
  }

  xSemaphoreTake(ctx.lock, portMAX_DELAY);
  if (ll_get_item(&ctx.sensor_list, calc_sensor_id(name), (void **)&sensor_item) == BmOK) {
    CLEAR_BIT(sensor_item->flags, ENABLED_FLAG);
    xTaskNotifyGive(sensor_item->handle);
    ret = true;
  }
  xSemaphoreGive(ctx.lock);

  return ret;
}

/*!
 @brief Enable A Sensor

 @details Sets the ENABLED_FLAG, allowing the sensor to be sampled in it's
          task. Notifies the task to indicate that the sampling has been
          affected.

 @param name Sensor string identifier

 @return true if disabled successfully, false otherwise
*/
bool sensorSamplerEnable(const char *name) {
  bool ret = false;
  SensorListItem *sensor_item = NULL;

  if (!name) {
    return ret;
  }

  xSemaphoreTake(ctx.lock, portMAX_DELAY);
  if (ll_get_item(&ctx.sensor_list, calc_sensor_id(name), (void **)&sensor_item) == BmOK) {
    SET_BIT(sensor_item->flags, ENABLED_FLAG);
    xTaskNotifyGive(sensor_item->handle);
    ret = true;
  }
  xSemaphoreGive(ctx.lock);

  return ret;
}

/*!
 @brief Disable Periodic Sensor Checks

 @details Could be useful during low power mode. Will delete the check task
          and set the handle to NULL. Will only disable if the task was
          previously enabled.

 @return true if successful, false otherwise
*/
bool sensorSamplerDisableChecks(void) {
  bool ret = false;

  xSemaphoreTake(ctx.lock, portMAX_DELAY);
  if (ctx.check_task) {
    vTaskDelete(ctx.check_task);
    ctx.check_task = NULL;
    ret = true;
  }
  xSemaphoreGive(ctx.lock);

  return ret;
}

/*!
 @brief Enable Periodic Sensor Checks 

 @details Will only enable if previously disabled. Starts the sensor check
          check task.

 @return true if successful, false otherwise
*/
bool sensorSamplerEnableChecks(void) {
  bool ret = false;

  xSemaphoreTake(ctx.lock, portMAX_DELAY);
  if (!ctx.check_task) {
    ret = start_check_task();
  }
  xSemaphoreGive(ctx.lock);

  return ret;
}

/*!
 @brief Obtain Sampling Period Of Sensor

 @param name Sensor string identifier

 @return Interval to sample sensor at in ms
 */
uint32_t sensorSamplerGetSamplingIntervalMs(const char *name) {
  uint32_t ret = 0;
  SensorListItem *sensor_item = NULL;

  if (!name) {
    return ret;
  }

  xSemaphoreTake(ctx.lock, portMAX_DELAY);
  if (ll_get_item(&ctx.sensor_list, calc_sensor_id(name), (void **)&sensor_item) == BmOK) {
    ret = sensor_item->sensor->intervalMs;
  }
  xSemaphoreGive(ctx.lock);

  return ret;
}

/*!
 @brief Change The Sample Period Of A Sensor

 @details If sensor is found, will notify the sensor task to indicate that
          sampling has been updated.

 @param name Sensor string identifier
 @param new_period_ms New period to sample the sensor in ms

 @return True on success, false on failure
 */
bool sensorSamplerChangeSamplingIntervalMs(const char *name, uint32_t new_period_ms) {
  bool ret = false;
  SensorListItem *sensor_item = NULL;

  if (!name) {
    return ret;
  }

  xSemaphoreTake(ctx.lock, portMAX_DELAY);
  if (ll_get_item(&ctx.sensor_list, calc_sensor_id(name), (void **)&sensor_item) == BmOK) {
    sensor_item->sensor->intervalMs = new_period_ms;
    // Notify task to inform it to change sample rate
    xTaskNotifyGive(sensor_item->handle);
    ret = true;
  }
  xSemaphoreGive(ctx.lock);

  return ret;
}
