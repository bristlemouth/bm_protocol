#pragma once
#include "bristlefin.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SENSORS_NUM_RETRIES 3

#define DEFAULT_SENSORS_POLL_MS 10000
#define DEFAULT_SENSORS_CHECK_S 60000

void sensorsInit();
extern Bristlefin bristlefin;

#ifdef __cplusplus
}
#endif
