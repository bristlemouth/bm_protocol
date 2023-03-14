#pragma once

#include <stdint.h>
#include "bsp.h"
#include "protected_spi.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  uint8_t number;
  SPIInterface_t* interface;
} DebugSPI_t;

void debugSPIInit(const DebugSPI_t *interfaceList, uint32_t numberOfInterfaces);

#ifdef __cplusplus
}
#endif
