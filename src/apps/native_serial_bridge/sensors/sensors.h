#pragma once

#include "bristlefin.h"

// Required by sensor_sampler library headers even when sensors are unused.
#define SENSORS_NUM_RETRIES 3
#define DEFAULT_SENSORS_POLL_MS 0
#define DEFAULT_SENSORS_CHECK_S 0

/// Global Bristlefin instance — controls LEDs and GPIO expander.
extern Bristlefin bristlefin;

void sensorsInit(void);
