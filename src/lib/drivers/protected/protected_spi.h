#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "FreeRTOS.h"
#include "semphr.h"
#include "io.h"
#include "stm32u5xx.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  SPI_OK = 0,
  SPI_TIMEOUT,
  SPI_MUTEX,
  SPI_ERR
} SPIResponse_t;

typedef struct {
  const char *name;
  SPI_HandleTypeDef * handle;
  void (*initFn)();
  SemaphoreHandle_t mutex;
} SPIInterface_t;

bool spiInit(SPIInterface_t *interface);
SPIResponse_t spiTxRx(SPIInterface_t *interface, IOPinHandle_t *csPin, size_t len, uint8_t *txBuff, uint8_t *rxBuff, uint32_t timeoutMs);
#define spiTx(interface, csPin, len, buff, timeout) spiTxRx(interface, csPin, len, buff, NULL, timeout);
#define spiRx(interface, csPin, len, buff, timeout) spiTxRx(interface, csPin, len, NULL, buff, timeout);

#ifdef __cplusplus
}
#endif

#define PROTECTED_SPI(name, handle, initFunction) {name, &handle, initFunction, NULL};
