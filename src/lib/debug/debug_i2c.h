#pragma once

#include <stdint.h>
#include "bsp.h"
#include "protected_i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  uint8_t number;
  I2CInterface_t* handle;
} DebugI2C_t;

void debugI2CInit(const DebugI2C_t *interfaceList, uint32_t numberOfInterfaces);

#ifdef __cplusplus
}
#endif
