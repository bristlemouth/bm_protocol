#pragma once
#include "bristlefin.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SENSORS_NUM_RETRIES 3

void sensorsInit();
extern Bristlefin bristlefin;

#ifdef __cplusplus
}
#endif
