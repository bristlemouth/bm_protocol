/**
 * @file bm_osal_freertos.c
 * @brief Bristlemouth OS Abstraction Layer for FreeRTOS.
 *
 * This file implements Bristlemouth OS abstraction layer for FreeRTOS, including mutexes, queues,
 * tasks, and timers.
 *
 * The following functions are currently implemented and available in the codebase:
 * - bm_osal_task_create()
 * - bm_osal_task_get_min_stack_size()
 * - bm_osal_task_delay()
 *
 * The following functions are implemented but not yet in use and will be available for future expansion:
 * - Mutex functions: bm_osal_mutex_*()
 * - Queue functions: bm_osal_queue_*()
 * - Timer functions: bm_osal_timer_*()
 *
 */

// Bristlelmouth includes
#include "bm_osal.h"

// FreeRTOS includes
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"
#include "timers.h"

// STM32 BSP includes
#include "iwdg.h"

// ====================================================
// Compile-Time Type Assertions (to validate OSAL assumptions)
// ====================================================

_Static_assert(sizeof(bm_osal_result_t) == sizeof(BaseType_t),
               "bm_osal_result_t must match size of BaseType_t");

_Static_assert(sizeof(bm_osal_tick_type_t) == sizeof(TickType_t),
               "bm_osal_tick_type_t must match size of TickType_t");

_Static_assert(sizeof(bm_osal_mutex_t) == sizeof(SemaphoreHandle_t),
               "bm_osal_mutex_t must match size of SemaphoreHandle_t");

_Static_assert(sizeof(bm_osal_queue_t) == sizeof(QueueHandle_t),
               "bm_osal_queue_t must match size of QueueHandle_t");

_Static_assert(sizeof(bm_osal_timer_t) == sizeof(TimerHandle_t),
               "bm_osal_timer_t must match size of TimerHandle_t");

_Static_assert(sizeof(bm_osal_task_handle_t) == sizeof(TaskHandle_t),
               "bm_osal_task_handle_t must match size of TaskHandle_t");

// ====================================================
// Mutex API
// ====================================================

bool bm_osal_mutex_create(bm_osal_mutex_t* mutex) {
    *mutex = xSemaphoreCreateMutex();
    return (*mutex != NULL);
}

bool bm_osal_mutex_lock(bm_osal_mutex_t mutex, uint32_t timeout_ms) {
    return (xSemaphoreTake((SemaphoreHandle_t)mutex, pdMS_TO_TICKS(timeout_ms)) == pdTRUE);
}

bool bm_osal_mutex_unlock(bm_osal_mutex_t mutex) {
    return (xSemaphoreGive((SemaphoreHandle_t)mutex) == pdTRUE);
}

bool bm_osal_mutex_delete(bm_osal_mutex_t mutex) {
    vSemaphoreDelete((SemaphoreHandle_t)mutex);
    return true;
}

// ====================================================
// Queue API
// ====================================================

bool bm_osal_queue_create(bm_osal_queue_t* queue, uint32_t length, uint32_t item_size) {
    *queue = xQueueCreate(length, item_size);
    return (*queue != NULL);
}

bool bm_osal_queue_send(bm_osal_queue_t queue, const void* item, uint32_t timeout_ms) {
    return (xQueueSend((QueueHandle_t)queue, item, pdMS_TO_TICKS(timeout_ms)) == pdTRUE);
}

bool bm_osal_queue_receive(bm_osal_queue_t queue, void* item, uint32_t timeout_ms) {
    return (xQueueReceive((QueueHandle_t)queue, item, pdMS_TO_TICKS(timeout_ms)) == pdTRUE);
}

bool bm_osal_queue_delete(bm_osal_queue_t queue) {
    vQueueDelete((QueueHandle_t)queue);
    return true;
}

// ====================================================
// Task API
// ====================================================

bool bm_osal_task_create(bm_osal_task_func_t task_func, const char* name, uint16_t stack_size,
        void* arg,  uint32_t priority, bm_osal_task_handle_t* task_handle) {
    return (xTaskCreate((TaskFunction_t)task_func, name, stack_size, arg, priority,
            (TaskHandle_t *)task_handle) == pdPASS);
}

// Deletes the calling task. Not usable to delete other tasks.
bool bm_osal_task_delete(void) {
    vTaskDelete(NULL);
    return true;
}

uint16_t bm_osal_task_get_min_stack_size() {
    return configMINIMAL_STACK_SIZE;
}

void bm_osal_task_delay(bm_osal_tick_type_t delay) {
    vTaskDelay((TickType_t)delay);
}

// ====================================================
// Timer API
// ====================================================

bool bm_osal_timer_create(bm_osal_timer_t* timer, const char* name, uint32_t period_ms,
        bool auto_reload, bm_osal_timer_callback_t callback, void* arg) {
    *timer = xTimerCreate(name, pdMS_TO_TICKS(period_ms), auto_reload ? pdTRUE : pdFALSE, arg,
            (TimerCallbackFunction_t)callback);
    return (*timer != NULL);
}

bool bm_osal_timer_start(bm_osal_timer_t timer) {
    return (xTimerStart((TimerHandle_t)timer, 0) == pdPASS);
}

bool bm_osal_timer_stop(bm_osal_timer_t timer) {
    return (xTimerStop((TimerHandle_t)timer, 0) == pdPASS);
}

bool bm_osal_timer_delete(bm_osal_timer_t timer) {
    return (xTimerDelete((TimerHandle_t)timer, 0) == pdPASS);
}

// ====================================================
// Delay API
// ====================================================

void bm_osal_delay_ms(uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}

// ====================================================
// Assert API
// ====================================================

#ifndef configASSERT
    #error "configASSERT must be defined in FreeRTOSConfig.h"
#endif

void bm_osal_assert(int condition) {
    configASSERT(condition);
}
