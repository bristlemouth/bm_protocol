//
// Created by Genton Mo on 1/18/21.
//

#pragma once

#include <stdint.h>
#include "serial.h"

#ifdef __cplusplus
extern "C" {
#endif

void debugMemfaultInit(SerialHandle_t *serialConsoleHandle);

#ifdef __cplusplus
}
#endif
