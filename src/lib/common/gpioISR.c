#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "bsp.h"
#include "debug.h"
#include "gpioISR.h"
#include "task_priorities.h"

#define MAX_GPIO_ISRS 16

static TaskHandle_t GPIOISRTaskHandle = NULL;
static xQueueHandle gpioISRQueue = NULL;

typedef struct {
  const IOPinHandle_t *pinHandle;
  bool value;
  GPIOISRCallbackFn callback;
} GPIOISR_t;

// Returns true if we're currently in an ISR
static inline bool inISR() {
  return ((SCB->ICSR & SCB_ICSR_VECTACTIVE_Msk) != 0x0);
}

/*!
  Callback called from ISR which schedules non-ISR callback
  in gpioISRTask

  \param[in] pinHandle - IO pin callback is for
  \param[in] value - pin value at time of interrupt
  \param[in] args - callback function to call (from non-isr context)
  \return none
*/
bool gpioISRCallbackFromISR(const void *pinHandle, uint8_t value, void *args) {
  GPIOISR_t isr = {
    .pinHandle = pinHandle,
    .value = value,
    .callback = (GPIOISRCallbackFn)args,
  };

  BaseType_t pxHigherPriorityTaskWoken = pdFALSE;
  xQueueSendFromISR(gpioISRQueue, &isr, &pxHigherPriorityTaskWoken);

  return pxHigherPriorityTaskWoken;
}

/*!
  Register gpio ISR callback

  \param[in] pinHandle - IO pin callback is for
  \param[in] callback - callback function to call (from non-isr context)
  \return none
*/
void gpioISRRegisterCallback(IOPinHandle_t *pinHandle, GPIOISRCallbackFn callback) {
  configASSERT(pinHandle);
  configASSERT(callback);
  IORegisterCallback(pinHandle, gpioISRCallbackFromISR, callback);
}

/*!
  GPIO ISR Task

  Task to handle GPIO interrupts outside the interrupt context.
  This allows for using all RTOS functions for any interrupts that
  don't need to be handled inside the interrupt itself.
*/
static void gpioISRTask() {
  BaseType_t rval;
  GPIOISR_t isr;

  for (;;) {
    rval = xQueueReceive(gpioISRQueue, &isr, portMAX_DELAY);
    configASSERT(rval == pdTRUE);
    configASSERT(isr.callback);
    isr.callback(isr.pinHandle, isr.value, NULL);
  }
}

/*!
  Start ISR task. Make sure stack is appropriately sized
*/
void gpioISRStartTask() {
  gpioISRQueue = xQueueCreate(MAX_GPIO_ISRS, sizeof(GPIOISR_t));
  configASSERT(gpioISRQueue);

  bool rval = xTaskCreate(
              gpioISRTask,
              "GPIO_ISR",
              128 * 2,
              NULL,
              GPIO_ISR_TASK_PRIORITY,
              &GPIOISRTaskHandle);
  configASSERT(rval == pdTRUE);
}
