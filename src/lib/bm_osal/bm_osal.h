// bm_osal.h

#ifndef BM_OSAL_H
#define BM_OSAL_H

#include <stdint.h>
#include <stdbool.h>

// Mutex
typedef void* bm_osal_mutex_t;
bool bm_osal_mutex_create(bm_osal_mutex_t* mutex);
bool bm_osal_mutex_lock(bm_osal_mutex_t mutex, uint32_t timeout_ms);
bool bm_osal_mutex_unlock(bm_osal_mutex_t mutex);
bool bm_osal_mutex_delete(bm_osal_mutex_t mutex);

// Queue
typedef void* bm_osal_queue_t;
bool bm_osal_queue_create(bm_osal_queue_t* queue, uint32_t length, uint32_t item_size);
bool bm_osal_queue_send(bm_osal_queue_t queue, const void* item, uint32_t timeout_ms);
bool bm_osal_queue_receive(bm_osal_queue_t queue, void* item, uint32_t timeout_ms);
bool bm_osal_queue_delete(bm_osal_queue_t queue);

// Task
typedef void * bm_task_handle_t;
typedef void (*bm_osal_task_func_t)(void* arg);
bool bm_osal_task_create(bm_osal_task_func_t task_func, const char* name, uint16_t stack_size, void* arg,  uint32_t priority, bm_task_handle_t task_handle);
bool bm_osal_task_delete(void);

// Timer
typedef void* bm_osal_timer_t;
typedef void (*bm_osal_timer_callback_t)(void* arg);
bool bm_osal_timer_create(bm_osal_timer_t* timer, const char* name, uint32_t period_ms, bool auto_reload, bm_osal_timer_callback_t callback, void* arg);
bool bm_osal_timer_start(bm_osal_timer_t timer);
bool bm_osal_timer_stop(bm_osal_timer_t timer);
bool bm_osal_timer_delete(bm_osal_timer_t timer);

// Delay
void bm_osal_delay_ms(uint32_t ms);

#endif // BM_OSAL_H
