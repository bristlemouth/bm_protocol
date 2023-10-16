#pragma once
#include <stdint.h>
#include <stdlib.h>
#include "FreeRTOS.h"
#include "timers.h"

namespace SensorWatchdog {

/*!
 * @brief Callback function for sensor watchdog
 * @param[in] arg The argument passed in when the sensor watchdog was added.
 * @return True if the callback is succesfully called, false otherwise.
 */
typedef bool (*sensor_watchdog_handler)(void * arg);

void SensorWatchdogInit();
void SensorWatchdogAdd(const char * id, uint32_t timeoutMs, sensor_watchdog_handler handler, uint32_t max_triggers, const char * logHandle = NULL);
void SensorWatchdogPet(const char * id);

} // namespace SensorWatchdog
