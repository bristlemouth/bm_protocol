#pragma once

#include <stdint.h>
#include "stm32u5xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

void debugMicInit(SAI_HandleTypeDef *saiHandle);

#ifdef __cplusplus
}
#endif
