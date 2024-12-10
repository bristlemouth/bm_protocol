#include "sensorWatchdog.h"
#include "spotter.h"
#include "semphr.h"
#include "timer_callback_handler.h"
#include "uptime.h"

namespace SensorWatchdog {

#define WATCHDOG_PET_TIMEOUT_MS (10)

static sensor_watchdog_t *_sensor_watchdog_list = NULL;
static SemaphoreHandle_t _sensor_watchdog_list_mutex = NULL;

static void _addSensorWatchdog(sensor_watchdog_t *cfg);
static void _timerCallback(TimerHandle_t xTimer);
static void _sensorWatchdogHandler(void *arg);

/*!
 * @brief Initialize the sensor watchdog.
 */
void SensorWatchdogInit(void) { _sensor_watchdog_list_mutex = xSemaphoreCreateMutex(); }

/*!
 * @brief Add a sensor watchdog.
 * @param[in] id The id of the sensor watchdog.
 * @param[in] timeoutMs The timeout in milliseconds.
 * @param[in] handler The callback function to be called when the sensor watchdog expires.
 * @param[in] max_triggers The maximum number of times the sensor watchdog can expire before it is stopped or 0 for unlimited triggers.
 * @param[in] logHandle The spotter BM log handle to log to. If NULL, no logging will be done.
 */
void SensorWatchdogAdd(const char *id, uint32_t timeoutMs, sensor_watchdog_handler handler,
                       uint32_t max_triggers, const char *logHandle) {
  configASSERT(handler);
  configASSERT(timeoutMs > 0);
  configASSERT(id);
  sensor_watchdog_t *cfg =
      static_cast<sensor_watchdog_t *>(pvPortMalloc(sizeof(sensor_watchdog_t)));
  configASSERT(cfg);
  cfg->_id = id;
  cfg->_timeoutMs = timeoutMs;
  cfg->_handler = handler;
  cfg->_logHandle = logHandle;
  cfg->_max_triggers = max_triggers;
  cfg->_triggerCount = 0;
  cfg->_timerHandle =
      xTimerCreate(cfg->_id, pdMS_TO_TICKS(cfg->_timeoutMs), pdTRUE, NULL, _timerCallback);
  cfg->_next = NULL;
  _addSensorWatchdog(cfg);
  configASSERT(xTimerStart(cfg->_timerHandle, 0) == pdTRUE);
}

/*!
 * @brief Refresh a sensor watchdog.
 * @param[in] id The id of the sensor watchdog to pet.
 */
void SensorWatchdogPet(const char *id) {
  configASSERT(id);
  configASSERT(xSemaphoreTake(_sensor_watchdog_list_mutex,
                              pdMS_TO_TICKS(WATCHDOG_PET_TIMEOUT_MS)) == pdTRUE);
  sensor_watchdog_t *curr = _sensor_watchdog_list;
  while (curr) {
    if (strcmp(curr->_id, id) == 0) {
      curr->_triggerCount = 0;
      configASSERT(xTimerReset(curr->_timerHandle, pdMS_TO_TICKS(WATCHDOG_PET_TIMEOUT_MS)) ==
                   pdTRUE);
      break;
    }
    curr = curr->_next;
  }
  xSemaphoreGive(_sensor_watchdog_list_mutex);
}

static void _addSensorWatchdog(sensor_watchdog_t *cfg) {
  configASSERT(cfg);
  xSemaphoreTake(_sensor_watchdog_list_mutex, portMAX_DELAY);
  if (_sensor_watchdog_list) {
    sensor_watchdog_t *curr = _sensor_watchdog_list;
    while (curr->_next) {
      curr = curr->_next;
    }
    curr->_next = cfg;
  } else {
    _sensor_watchdog_list = cfg;
  }
  xSemaphoreGive(_sensor_watchdog_list_mutex);
}

static void _timerCallback(TimerHandle_t xTimer) {
  configASSERT(xTimer);
  configASSERT(xSemaphoreTake(_sensor_watchdog_list_mutex, 0) == pdTRUE);
  sensor_watchdog_t *curr = _sensor_watchdog_list;
  while (curr) {
    if (curr->_timerHandle == xTimer) {
      configASSERT(timer_callback_handler_send_cb(_sensorWatchdogHandler,
                                                  reinterpret_cast<void *>(curr), 0) == pdTRUE);
    }
    curr = curr->_next;
  }
  xSemaphoreGive(_sensor_watchdog_list_mutex);
}

static void _sensorWatchdogHandler(void *arg) {
  configASSERT(arg);
  sensor_watchdog_t *curr = reinterpret_cast<sensor_watchdog_t *>(arg);
  if (curr->_logHandle) {
    spotter_log(0, curr->_logHandle, USE_TIMESTAMP, "[%s] | tick: %" PRIu64 " SensorWatchdog Expired!\n",
               curr->_id, uptimeGetMs());
  }
  printf("[%s] | tick: %" PRIu64 " SensorWatchdog Expired!\n", curr->_id, uptimeGetMs());
  configASSERT(xTimerStop(curr->_timerHandle, 0) == pdTRUE);
  curr->_handler(arg);
  configASSERT(xTimerReset(curr->_timerHandle, pdMS_TO_TICKS(WATCHDOG_PET_TIMEOUT_MS)) ==
               pdTRUE);
  if (curr->_max_triggers) {
    if (curr->_triggerCount < curr->_max_triggers) {
      curr->_triggerCount++;
    } else {
      configASSERT(xTimerStop(curr->_timerHandle, 0) == pdTRUE);
      if (curr->_logHandle) {
        spotter_log(0, curr->_logHandle, USE_TIMESTAMP,
                   "[%s] | tick: %" PRIu64 " SensorWatchdog max trigger: %" PRIu32
                   " reached!\n",
                   curr->_id, uptimeGetMs(), curr->_max_triggers);
      }
      printf("[%s] | tick: %" PRIu64 " SensorWatchdog max trigger: %" PRIu32 " reached!\n",
             curr->_id, uptimeGetMs(), curr->_max_triggers);
    }
  } else {
    curr->_triggerCount++;
  }
}

} // namespace SensorWatchdog
