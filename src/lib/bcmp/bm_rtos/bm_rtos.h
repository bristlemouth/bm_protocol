#ifndef BM_RTOS_H
#define BM_RTOS_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

// base it off the posix error codes?
typedef enum BmError {
  BM_SUCCESS = 0,
  BM_FAIL = -1,
} BmError;

// Something like this for the asserts? Although maybe this shouldn't live here
// #ifdef DEV_MODE
// #define ASSERT(x) configASSERT(x)
// #else
// #define ASSERT(x) ((void)0)
// #endif

typedef void *BmQueue;
typedef void *BmSemaphore;
typedef void *BmTimer;

// Memory functions - these may not necessarily be tied to the RTOS but I'm including them here for now
void *bm_malloc(size_t size);
void bm_free(void *ptr);

// Queue functions
BmQueue bm_queue_create(uint32_t queue_length, uint32_t item_size);
BmError bm_queue_receive(BmQueue queue, void *item, uint32_t timeout_ms);
BmError bm_queue_send(BmQueue queue, const void *item, uint32_t timeout_ms);

// Semaphore functions
BmSemaphore bm_semaphore_create(void);
BmError bm_semaphore_give(BmSemaphore semaphore);
BmError bm_semaphore_take(BmSemaphore semaphore, uint32_t timeout_ms);

// Task functions
BmError bm_task_create(void (*task)(void *), const char *name, uint32_t stack_size, void *arg,
                       uint32_t priority, void *task_handle);
void bm_start_scheduler(void);

// Timer functions
BmTimer bm_timer_create(void (*callback)(void *), const char *name, uint32_t period_ms,
                        void *arg);
BmError bm_timer_start(BmTimer timer, uint32_t timeout_ms);
BmError bm_timer_stop(BmTimer timer, uint32_t timeout_ms);
BmError bm_timer_change_period(BmTimer timer, uint32_t period_ms, uint32_t timeout_ms);
uint32_t bm_get_tick_count(void);
uint32_t bm_ms_to_ticks(uint32_t ms);
uint32_t bm_ticks_to_ms(uint32_t ticks);
void bm_delay(uint32_t ms);

// These are the functions that I need to implement in the FreeRTOS+CLI porting layer... maybe???
// FreeRTOS_CLIGetParameter
// FreeRTOS_CLIRegisterCommand

#ifdef __cplusplus
}
#endif

#endif // BM_RTOS_H