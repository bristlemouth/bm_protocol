#pragma once

// Include the shared sensor app interface header
#include "sensor_app_user.h"  // This is from src/lib/sensor_app_interface/

// Global sensor app instance
extern SensorAppUser app;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the sensor application (platform-specific implementation)
 */
void setup(void);

/**
 * @brief Main sensor application loop (delegates to app.loop())
 */
void loop(void);

#ifdef __cplusplus
}
#endif