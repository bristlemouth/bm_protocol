#pragma once

#include "FreeRTOS.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef bool (*IOWriteFn)(const void *pinHandle, uint8_t value);
typedef bool (*IOReadFn)(const void *pinHandle, uint8_t *value);
typedef bool (*IOConfigFn)(const void *pinHandle, const void *config);

// Callback returns true if "higherPriorityTaskWoken"
typedef bool (*IOCallbackFn)(const void *pinHandle, uint8_t value, void *args);
typedef bool (*IORegisterCallbackFn)(const void *pinHandle, IOCallbackFn callback, void *config);

// Structure to describe GPIO driver. It should point to functions that read,
// write, configure the pins. All functions take a pinHandle that describes
// the IO pin in a way that's specific to the driver.
typedef struct {
  IOWriteFn write;
  IOReadFn read;
  IOConfigFn config;
  IORegisterCallbackFn registerCallback;
  // Add more generic functions here
} IODriver_t;

typedef const void * IOPin;

typedef struct {
  const IODriver_t *driver;
  const IOPin pin;
} IOPinHandle_t;

typedef void * IOConfig;

static inline bool IOConfigure(IOPinHandle_t *pinHandle, IOConfig configuration){
  configASSERT(pinHandle != NULL);
  configASSERT(pinHandle->driver != NULL);
  return pinHandle->driver->config(pinHandle->pin, configuration);
}

// TODO - add interrupt configuration/enable/handle

static inline bool IOWrite(IOPinHandle_t *pinHandle, uint8_t value) {
  configASSERT(pinHandle != NULL);
  configASSERT(pinHandle->driver != NULL);
  return pinHandle->driver->write(pinHandle->pin, value);
}

static inline bool IORead(IOPinHandle_t *pinHandle, uint8_t *value) {
  configASSERT(pinHandle != NULL);
  configASSERT(pinHandle->driver != NULL);
  return pinHandle->driver->read(pinHandle->pin, value);
}

static inline bool IORegisterCallback(IOPinHandle_t *pinHandle, const IOCallbackFn callback, void *args) {
  configASSERT(pinHandle != NULL);
  configASSERT(pinHandle->driver != NULL);
  if(pinHandle->driver->registerCallback) {
    return pinHandle->driver->registerCallback(pinHandle->pin, callback, args);
  } else {
    return false;
  }
}

#ifdef __cplusplus
}
#endif
