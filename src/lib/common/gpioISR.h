#pragma once

#include <stdint.h>
#include <stdio.h>
#include "io.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef bool (*GPIOISRCallbackFn)(const void *pinHandle, uint8_t value, void *args);

void gpioISRStartTask();
void gpioISRRegisterCallback(IOPinHandle_t *pinHandle, GPIOISRCallbackFn callback);
bool gpioISRCallbackFromISR(const void *pinHandle, uint8_t value, void *args);

#ifdef __cplusplus
}
#endif
