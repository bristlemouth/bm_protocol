#pragma once

#include "FreeRTOS.h"
#include "io.h"

#ifdef STM32L496xx
#include "stm32l4xx_ll_gpio.h"
#endif

#ifdef STM32U575xx
#include "stm32u5xx_ll_gpio.h"

#endif

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
