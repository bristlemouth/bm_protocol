// maybe use the define guard here instead of pragma once since pragma is not officially supported in C++?
#pragma once

#ifdef __cplusplus
extern "C"
{
#endif




// Something like this for the asserts? Although maybe this shouldn't live here
// #ifdef DEV_MODE
// #define ASSERT(x) configASSERT(x)
// #else
// #define ASSERT(x) ((void)0)
// #endif

// TODO - give this a better name
// also BaseType_t is defined in FreeRTOS so perhaps it this needs to be defined in the implementaiton file?
// in FreeRTOS Basetype_t is defined as long. I'm not sure what the equivalent might be defined as in other RTOSs.
typedef bm_return_value_t BaseType_t;

typedef void *bm_queue_handle_t;
typedef void *bm_semaphore_handle_t;
typedef void *bm_timer_handle_t;

// Memory functions
void *bm_malloc(size_t size);
void bm_free(void *ptr);

// Queue functions
bm_queue_handle_t bm_queue_create(uint32_t queue_length, uint32_t item_size);
bm_return_value_t bm_queue_receive(bm_queue_handle_t queue, void *item, uint32_t timeout_ms);
bm_return_value_t bm_queue_send(bm_queue_handle_t queue, const void *item, uint32_t timeout_ms);

// Semaphore functions
bm_semaphore_handle_t bm_semaphore_create(void);
bm_return_value_t bm_semaphore_give(bm_semaphore_handle_t semaphore);
bm_return_value_t bm_semaphore_take(bm_semaphore_handle_t semaphore, uint32_t timeout_ms);

// Task functions
bm_return_value_t bm_task_create(void (*task)(void *), const char *name, uint32_t stack_size, void *arg);
// Going to need to add a function to start the scheduler here!

// Timer functions
bm_timer_handle_t bm_timer_create(void (*callback)(void *), const char *name, uint32_t period_ms, void *arg);
bm_return_value_t bm_timer_start(bm_timer_handle_t timer, uint32_t period_ms);
bm_return_value_t bm_timer_stop(bm_timer_handle_t timer, uint32_t timeout_ms);
bm_return_value_t bm_timer_change_period(bm_timer_handle_t timer, uint32_t period_ms);
uint32_t bm_get_tick_count(void);
uint32_t bm_ms_to_ticks(uint32_t ms);
uint32_t bm_ticks_to_ms(uint32_t ticks);
void bm_delay(uint32_t ms);


// These are the functions that I need to implement in the FreeRTOS porting layer
// configASSERT
// pvPortMalloc
// vPortFree
// vTaskDelay
// xQueueCreate
// xQueueReceive
// xQueueSend
// xSemaphoreCreateMutex
// xSemaphoreGive
// xSemaphoreTake
// xTaskCreate
// xTaskGetTickCount
// xTimerChangePeriod
// xTimerCreate
// xTimerStart
// xTimerStop
// pdMS_TO_TICKS
// pdTICKS_TO_MS


// These are the functions that I need to implement in the FreeRTOS+CLI porting layer... maybe???
// FreeRTOS_CLIGetParameter
// FreeRTOS_CLIRegisterCommand

#ifdef __cplusplus
}
#endif