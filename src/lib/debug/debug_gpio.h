#pragma once

#include <stdint.h>
#include "io.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  GPIO_IN = 0,
  GPIO_OUT,
  // TODO - add GPIO_OUT_OD?
} DebugGpioMode_t;

typedef struct {
  const char *name;
  IOPinHandle_t *handle;
  DebugGpioMode_t mode;
} DebugGpio_t;

void debugGpioInit(const DebugGpio_t *pinList, uint32_t numberOfPins);
const DebugGpio_t * findGpio(const char *name, uint32_t nameLen);

#ifdef __cplusplus
}
#endif
