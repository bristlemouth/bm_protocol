#pragma once

#include "FreeRTOS.h"
#include "io.h"
#include "protected_i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PCA9535_NUM_CHANNELS 16

typedef struct {
	I2CInterface_t *i2cDeviceHandle;
	uint8_t address;
	uint16_t outputPort;
	uint16_t inputPort;
	uint16_t configPort;
	IOCallbackFn callbacks[PCA9535_NUM_CHANNELS];
	bool enabled;
} PCA9535Device_t;

typedef struct {
  PCA9535Device_t *device;
  uint16_t pin;
  uint8_t mode;
} PCA9535Pin_t;

#define PCA9535_MODE_INPUT 1
#define PCA9535_MODE_OUTPUT 0

// TODO - make generic so we can have multiple devices with the same driver
bool pca9535Init(PCA9535Device_t *device);

void pca9535StartIRQTask();
bool pca9535IRQHandler(const void *pcaPinHandle, uint8_t value, void *args);

extern IODriver_t PCA9535PinDriver;

#ifdef __cplusplus
}
#endif
