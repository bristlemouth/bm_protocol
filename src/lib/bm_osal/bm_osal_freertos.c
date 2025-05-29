// bm_osal_freertos.c

#include "bm_osal.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"
#include "timers.h"

// Mutex
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

// Queue
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

// Task
bool bm_osal_task_create(bm_osal_task_func_t task_func, const char* name, uint16_t stack_size, void* arg,  uint32_t priority, bm_task_handle_t task_handle) {
    return (xTaskCreate(task_func, name, stack_size, arg, priority, task_handle) == pdPASS);
}

bool bm_osal_task_delete(void) {
    vTaskDelete(NULL);
    return true;
}

// Timer
bool bm_osal_timer_create(bm_osal_timer_t* timer, const char* name, uint32_t period_ms, bool auto_reload, bm_osal_timer_callback_t callback, void* arg) {
    *timer = xTimerCreate(name, pdMS_TO_TICKS(period_ms), auto_reload ? pdTRUE : pdFALSE, arg, (TimerCallbackFunction_t)callback);
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

// Delay
void bm_osal_delay_ms(uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}
