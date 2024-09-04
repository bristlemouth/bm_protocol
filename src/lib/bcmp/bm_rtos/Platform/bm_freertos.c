#include "bm_rtos.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "timers.h"

#define bmRETPASS pdPASS
#define bmRETFAIL pdFAIL

void *bm_malloc(size_t size) {
    return pvPortMalloc(size);
}

void bm_free(void *ptr) {
    vPortFree(ptr);
}

bm_queue_handle_t bm_queue_create(uint32_t queue_length, uint32_t item_size) {
    return xQueueCreate(queue_length, item_size);
}

bm_return_value_t bm_queue_receive(bm_queue_handle_t queue, void *item, uint32_t timeout_ms) {
    return xQueueReceive(queue, item, pdMS_TO_TICKS(timeout_ms));
}

bm_return_value_t bm_queue_send(bm_queue_handle_t queue, const void *item, uint32_t timeout_ms) {
    return xQueueSend(queue, item, pdMS_TO_TICKS(timeout_ms));
}

bm_semaphore_handle_t bm_semaphore_create(void) {
    return xSemaphoreCreateMutex();
}

bm_return_value_t bm_semaphore_give(bm_semaphore_handle_t semaphore) {
    return xSemaphoreGive(semaphore);
}

bm_return_value_t bm_semaphore_take(bm_semaphore_handle_t semaphore, uint32_t timeout_ms) {
    return xSemaphoreTake(semaphore, pdMS_TO_TICKS(timeout_ms));
}

bm_return_value_t bm_task_create(void (*task)(void *), const char *name, uint32_t stack_size, void *arg) {
   return xTaskCreate(task, name, stack_size, arg, tskIDLE_PRIORITY, NULL);
}

bm_timer_handle_t bm_timer_create(void (*callback)(void *), const char *name, uint32_t period_ms, void *arg) {
    return xTimerCreate(name, pdMS_TO_TICKS(period_ms), pdTRUE, arg, callback);
}

bm_return_value_t bm_timer_start(bm_timer_handle_t timer, uint32_t period_ms) {
    return xTimerStart(timer, pdMS_TO_TICKS(period_ms));
}

bm_return_value_t bm_timer_stop(bm_timer_handle_t timer, uint32_t timeout_ms) {
    return xTimerStop(timer, pdMS_TO_TICKS(timeout_ms));
}

bm_return_value_t bm_timer_change_period(bm_timer_handle_t timer, uint32_t period_ms) {
    return xTimerChangePeriod(timer, pdMS_TO_TICKS(period_ms), pdMS_TO_TICKS(0));
}

uint32_t bm_get_tick_count(void) {
    return xTaskGetTickCount();
}

uint32_t bm_ms_to_ticks(uint32_t ms) {
    return pdMS_TO_TICKS(ms);
}

uint32_t bm_ticks_to_ms(uint32_t ticks) {
    return pdTICKS_TO_MS(ticks);
}

void bm_delay(uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}