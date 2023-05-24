#pragma once
#include <stdbool.h>
#include "serial.h"

#ifdef __cplusplus
extern "C" {
#endif

bool micropython_freertos_init(SerialHandle_t *serial_handle);

#ifdef __cplusplus
}
#endif
