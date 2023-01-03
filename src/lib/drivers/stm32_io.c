
#include "stm32_io.h"

static bool STM32IOWriteFn(const void *pinHandle, uint8_t value);
static bool STM32IOReadFn(const void *pinHandle, uint8_t *value);
static bool STM32IOConfigFn(const void *pinHandle, const void *config);
static bool STM32IORegisterCallback(const void *pinHandle, const IOCallbackFn callback, void *config);

IODriver_t STM32PinDriver = {
  .write = STM32IOWriteFn,
  .read = STM32IOReadFn,
  .config = STM32IOConfigFn,
  .registerCallback = STM32IORegisterCallback,
};

static bool STM32IOWriteFn(const void *pinHandle, uint8_t value) {
  configASSERT(pinHandle != NULL);
  STM32Pin_t *pin = (STM32Pin_t *)pinHandle;

  if(value) {
    LL_GPIO_SetOutputPin(pin->gpio, pin->pinmask);
  } else {
    LL_GPIO_ResetOutputPin(pin->gpio, pin->pinmask);
  }
  return true;
}

static bool STM32IOReadFn(const void *pinHandle, uint8_t *value) {
  configASSERT(pinHandle != NULL);
  configASSERT(value != NULL);
  STM32Pin_t *pin = (STM32Pin_t *)pinHandle;
  *value = LL_GPIO_IsInputPinSet(pin->gpio, pin->pinmask);

  return true;
}

static bool STM32IOConfigFn(const void *pinHandle, const void *config) {
  configASSERT(pinHandle != NULL);
  configASSERT(config != NULL);
  STM32Pin_t *pin = (STM32Pin_t *)pinHandle;
  LL_GPIO_InitTypeDef *gpio_InitStruct = (LL_GPIO_InitTypeDef *)config;

  if(LL_GPIO_Init(pin->gpio, gpio_InitStruct) == SUCCESS) {
    return true;
  } else {
    return false;
  }
}

static bool STM32IORegisterCallback(const void *pinHandle, const IOCallbackFn callback, void *args) {
  configASSERT(pinHandle != NULL);
  STM32Pin_t *pin = (STM32Pin_t *)pinHandle;

  pin->callback = callback;
  pin->args = args;

  return true;
}

bool STM32IOHandleInterrupt(const STM32Pin_t *pin) {
  configASSERT(pin != NULL);
  if (pin->callback) {
    return pin->callback(pin, LL_GPIO_IsInputPinSet(pin->gpio, pin->pinmask), pin->args);
  } else {
    return false;
  }
}
