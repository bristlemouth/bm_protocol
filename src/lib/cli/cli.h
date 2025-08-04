#pragma once

#include <stdint.h>
#include "serial.h"
#include "FreeRTOS.h"
#include "queue.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  uint8_t *buff;
  uint16_t len;
  SerialHandle_t *src;
} CLICommand_t;

void startCLI();
xQueueHandle cliGetQueue();

#ifdef __cplusplus
}
#endif
