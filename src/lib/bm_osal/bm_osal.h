/**
 * @file bm_osal.h
 * @brief RTOS Abstraction Layer for Bristlemouth.
 *
 * This header defines the OS abstraction layer for Bristlemouth.
 * It standardizes APIs for mutexes, queues, tasks, timers, and delays,
 * allowing code to be portable across different RTOS implementations.
 */

#ifndef BM_OSAL_H
#define BM_OSAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/**
 * @defgroup OSAL OS Abstraction Layer
 * @brief OS-level API for task, timer, mutex, and queue abstraction.
 * @{
 */

// ====================================================
// @name Core Types and Constants
// ====================================================

/** @brief Base return type for OSAL functions. */
typedef int bm_osal_base_t;

/** @brief Tick type used for timeouts and delays. */
typedef uint32_t bm_osal_tick_type_t;

/** @brief Result type returned by most OSAL APIs. */
typedef bm_osal_base_t bm_osal_result_t;

/** @brief Constant return value indicating success (maps to pdTRUE). */
extern const bm_osal_result_t BM_OSAL_TRUE;

/** @brief Constant return value indicating failure (maps to pdFALSE). */
extern const bm_osal_result_t BM_OSAL_FALSE;

// ====================================================
// @name Opaque Handle Types for OS Primitives
// ====================================================

/** @brief Opaque handle to a mutex object. */
typedef void* bm_osal_mutex_t;

/** @brief Opaque handle to a queue object. */
typedef void* bm_osal_queue_t;

/** @brief Opaque handle to a timer object. */
typedef void* bm_osal_timer_t;

/** @brief Opaque handle to a task object. */
typedef void* bm_osal_task_handle_t;

// ====================================================
// @name Function prototypes
// ====================================================

/** @brief Function signature for timer callbacks. */
typedef void (*bm_osal_timer_callback_t)(bm_osal_timer_t);

/** @brief Task function signature. */
typedef void (*bm_osal_task_func_t)(void*);

// ====================================================
// @name Mutex API
// ====================================================

/** @brief Create a mutex. */
bool bm_osal_mutex_create(bm_osal_mutex_t* mutex);

/** @brief Lock a mutex, blocking up to timeout_ms. */
bool bm_osal_mutex_lock(bm_osal_mutex_t mutex, uint32_t timeout_ms);

/** @brief Unlock a mutex. */
bool bm_osal_mutex_unlock(bm_osal_mutex_t mutex);

/** @brief Delete a mutex. */
bool bm_osal_mutex_delete(bm_osal_mutex_t mutex);

// ====================================================
// @name Queue API
// ====================================================

/** @brief Create a queue of specified length and item size. */
bool bm_osal_queue_create(bm_osal_queue_t* queue, uint32_t length, uint32_t item_size);

/** @brief Send an item to a queue. */
bool bm_osal_queue_send(bm_osal_queue_t queue, const void* item, uint32_t timeout_ms);

/** @brief Receive an item from a queue. */
bool bm_osal_queue_receive(bm_osal_queue_t queue, void* item, uint32_t timeout_ms);

/** @brief Delete a queue. */
bool bm_osal_queue_delete(bm_osal_queue_t queue);

// ====================================================
// @name Task API
// ====================================================

/** @brief Create a task. */
bool bm_osal_task_create(bm_osal_task_func_t task_func, const char* name, uint16_t stack_size,
                         void* arg, uint32_t priority, bm_osal_task_handle_t* task_handle);

/** @brief Delete the calling task. */
bool bm_osal_task_delete(void);

/** @brief Get minimum stack size needed for a task. */
uint16_t bm_osal_task_get_min_stack_size(void);

/** @brief Delay the current task for the specified duration. */
void bm_osal_task_delay(bm_osal_tick_type_t delay);

// ====================================================
// @name Timer API
// ====================================================

/** @brief Create a software timer. */
bool bm_osal_timer_create(bm_osal_timer_t* timer, const char* name, uint32_t period_ms,
                          bool auto_reload, bm_osal_timer_callback_t callback, void* arg);

/** @brief Start a software timer. */
bool bm_osal_timer_start(bm_osal_timer_t timer);

/** @brief Stop a software timer. */
bool bm_osal_timer_stop(bm_osal_timer_t timer);

/** @brief Delete a software timer. */
bool bm_osal_timer_delete(bm_osal_timer_t timer);

// ====================================================
// @name Delay API
// ====================================================

/** @brief Delay execution for a number of milliseconds. */
void bm_osal_delay_ms(uint32_t ms);

// ====================================================
// @name Assert API
// ====================================================

/** @brief Wrapper around the RTOS assertion mechanism. */
void bm_osal_assert(int condition);

/** @brief Assertion macro for convenience and future expansion. */
#define BM_OSAL_ASSERT(x) (bm_osal_assert((x)))

/** @} */ // End of OSAL group

#ifdef __cplusplus
}
#endif

#endif // BM_OSAL_H
