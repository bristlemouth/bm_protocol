#pragma once
#include "bristlefin.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SENSORS_NUM_RETRIES 3

#define DEFAULT_SENSORS_POLL_MS 0
#define DEFAULT_SENSORS_CHECK_S 0

void sensorsInit();
extern Bristlefin bristlefin;

#ifdef __cplusplus
}
#endif
