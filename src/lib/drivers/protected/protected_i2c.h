#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "FreeRTOS.h"
#include "semphr.h"
#include "bsp.h"

#define I2C_WORKAROUND 1

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	I2C_OK = 0,
	I2C_TIMEOUT,
	I2C_NACK,
	I2C_MUTEX,
	I2C_ERR
} I2CResponse_t;

typedef struct {
	const char *name;
	void *handle;
	void (*initFn)();
	SemaphoreHandle_t mutex;
} I2CInterface_t;

bool i2cInit(I2CInterface_t *interface);
I2CResponse_t i2cTxRx(I2CInterface_t *interface, uint8_t address, uint8_t *txBuff, size_t txLen, uint8_t *rxBuff, size_t rxLen, uint32_t timeoutMs);
#define i2cTx(interface, address, buff, len, timeout) i2cTxRx(interface, address, buff, len, NULL, 0, timeout);
#define i2cRx(interface, address, buff, len, timeout) i2cTxRx(interface, address, NULL, 0, buff, len, timeout);
I2CResponse_t i2cProbe(I2CInterface_t *interface, uint8_t address, uint32_t timeoutMs);
void i2cLoadLogCfg();

#ifdef __cplusplus
}
#endif

#define PROTECTED_I2C(name, handle, initFunction) {name, &handle, initFunction, NULL};
