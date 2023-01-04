#pragma once

#include "FreeRTOS.h"
#include "io.h"
#include "bsp.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  GPIO_TypeDef *gpio;
  uint32_t pinmask;
  IOCallbackFn callback;
  void *args;
} STM32Pin_t;

bool STM32IOHandleInterrupt(const STM32Pin_t *pin);

extern IODriver_t STM32PinDriver;

#ifdef __cplusplus
}
#endif
