
#include "stm32_io.h"

bool STM32IOWriteFn(const void *pinHandle, uint8_t value);
bool STM32IOReadFn(const void *pinHandle, uint8_t *value);
bool STM32IOConfigFn(const void *pinHandle, const void *config);

IODriver_t STM32PinDriver = {
  .write = STM32IOWriteFn,
  .read = STM32IOReadFn,
  .config = STM32IOConfigFn
};

bool STM32IOWriteFn(const void *pinHandle, uint8_t value) {
  configASSERT(pinHandle != NULL);
  (void)value;

  // Do nothing for now

  return true;
}

bool STM32IOReadFn(const void *pinHandle, uint8_t *value) {
  configASSERT(pinHandle != NULL);
  configASSERT(value != NULL);

  // Do nothing for now

  return true;
}

bool STM32IOConfigFn(const void *pinHandle, const void *config) {
  configASSERT(pinHandle != NULL);
  configASSERT(config != NULL);

  // Do nothing for now

  return true;
}
