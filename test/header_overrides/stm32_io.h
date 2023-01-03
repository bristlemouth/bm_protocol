#pragma once

#include "FreeRTOS.h"
#include "io.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  void *gpio;
  uint32_t pinmask;
} STM32Pin_t;

extern IODriver_t STM32PinDriver;

#ifdef __cplusplus
}
#endif
