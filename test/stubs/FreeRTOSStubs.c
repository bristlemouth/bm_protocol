
#include "FreeRTOS.h"
#include "task.h"
#include <stdlib.h>

static TickType_t _current_ticks = 0;

void vTaskDelay(const TickType_t xTicksToDelay) {
  _current_ticks = _current_ticks + xTicksToDelay;
}

TickType_t xTaskGetTickCount() { return _current_ticks; }

void xTaskSetTickCount(uint32_t xCurrentTickCount) {
  _current_ticks = (TickType_t)xCurrentTickCount;
}

// Stubs for FreeRTOS task functions used by queue.c
void vTaskSuspendAll(void) {}
BaseType_t xTaskResumeAll(void) { return pdTRUE; }
void vTaskMissedYield(void) {}
void vTaskPlaceOnEventList(List_t *pxEventList, TickType_t xTicksToWait) {
  (void)pxEventList;
  (void)xTicksToWait;
}
void vTaskPlaceOnEventListRestricted(List_t *pxEventList, TickType_t xTicksToWait,
                                     const BaseType_t xWaitIndefinitely) {
  (void)pxEventList;
  (void)xTicksToWait;
  (void)xWaitIndefinitely;
}
BaseType_t xTaskRemoveFromEventList(const List_t *pxEventList) {
  (void)pxEventList;
  return pdFALSE;
}
void vTaskInternalSetTimeOutState(TimeOut_t *pxTimeOut) { (void)pxTimeOut; }
BaseType_t xTaskCheckForTimeOut(TimeOut_t *pxTimeOut, TickType_t *pxTicksToWait) {
  (void)pxTimeOut;
  (void)pxTicksToWait;
  return pdFALSE;
}
void vTaskPriorityDisinheritAfterTimeout(TaskHandle_t const pxMutexHolder,
                                         UBaseType_t uxHighestPriorityWaitingTask) {
  (void)pxMutexHolder;
  (void)uxHighestPriorityWaitingTask;
}
TaskHandle_t pvTaskIncrementMutexHeldCount(void) { return NULL; }
UBaseType_t uxTaskGetNumberOfTasks(void) { return 0; }
TaskHandle_t xTaskGetCurrentTaskHandle(void) { return NULL; }
BaseType_t xTaskGetSchedulerState(void) { return taskSCHEDULER_RUNNING; }
BaseType_t xTaskPriorityDisinherit(TaskHandle_t const pxMutexHolder) {
  (void)pxMutexHolder;
  return pdFALSE;
}
BaseType_t xTaskPriorityInherit(TaskHandle_t const pxMutexHolder) {
  (void)pxMutexHolder;
  return pdFALSE;
}

// Memory allocation stubs
// Undefine the macros from FreeRTOSConfig.h to avoid infinite recursion
#undef pvPortMalloc
#undef vPortFree

void *pvPortMalloc(size_t xWantedSize) { return malloc(xWantedSize); }
void vPortFree(void *pv) { free(pv); }
