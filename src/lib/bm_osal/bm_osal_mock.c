/**
 * @file bm_osal_mock.c
 * @brief Dummy mock of Bristlemouth OSAL for unit testing (no FreeRTOS or POSIX required).
 *
 * This mock implementation is used to allow unit tests to compile and link without a real OS.
 * All functions return fixed "success" responses and do not perform actual threading or timing.
 */

#include "bm_osal.h"
#include "bm_osal_mock_defs.h"

// Simulate OSAL "success" and "failure" constants
const bm_osal_result_t BM_OSAL_TRUE  = 1;
const bm_osal_result_t BM_OSAL_FALSE = 0;

#define BM_OSAL_NOT_IMPLEMENTED(name) \
    do { fprintf(stderr, "[BM_OSAL_MOCK] %s not implemented\n", name); } while (0)

// ====================================================
// Mutex API
// ====================================================

bool bm_osal_mutex_create(bm_osal_mutex_t* mutex) {
    // Set dummy non-null value
    *mutex = (void*)BM_OSAL_DUMMY_MUTEX_HANDLE;
    return true;
}

bool bm_osal_mutex_lock(bm_osal_mutex_t mutex, uint32_t timeout_ms) {
    (void)mutex;
    (void)timeout_ms;
    return true;
}

bool bm_osal_mutex_unlock(bm_osal_mutex_t mutex) {
    (void)mutex;
    return true;
}

bool bm_osal_mutex_delete(bm_osal_mutex_t mutex) {
    (void)mutex;
    return true;
}

// ====================================================
// Queue API
// ====================================================

// typedef struct {
//     uint8_t buffer[10][sizeof(int)];
//     int head;
//     int tail;
//     int count;
// } mock_queue_t;

// typedef mock_queue_t* bm_osal_queue_t;

bm_osal_base_t bm_osal_queue_create(bm_osal_queue_t* queue, uint32_t length, uint32_t item_size) {
    (void)length;
    (void)item_size;
    *queue = BM_OSAL_DUMMY_QUEUE_HANDLE;
    return true;
}

bm_osal_base_t bm_osal_queue_send(bm_osal_queue_t queue, const void* item, uint32_t timeout_ms) {
    (void)queue;
    (void)item;
    (void)timeout_ms;
    return true;
}

bm_osal_base_t bm_osal_queue_receive(bm_osal_queue_t queue, void* item, uint32_t timeout_ms) {
    (void)queue;
    (void)item;
    (void)timeout_ms;
    return true;
}

bm_osal_base_t bm_osal_queue_delete(bm_osal_queue_t queue) {
    (void)queue;
    return true;
}

// ====================================================
// Task API
// ====================================================

static uint16_t _stack_size;

bm_osal_base_t bm_osal_task_create(bm_osal_task_func_t task_func, const char* name, uint16_t stack_size,
                         void* arg, uint32_t priority, bm_osal_task_handle_t* task_handle) {
    (void)task_func;
    (void)name;
    (void)arg;
    (void)priority;
    *task_handle = BM_OSAL_DUMMY_TASK_HANDLE;
    _stack_size = stack_size;
    return true;
}

bm_osal_base_t bm_osal_task_delete(void) {
    return true;
}

uint16_t bm_osal_task_get_min_stack_size(void) {
    return _stack_size;
}

void bm_osal_task_delay(bm_osal_tick_type_t delay) {
    (void)delay;
}

// ====================================================
// Timer API
// ====================================================

bool bm_osal_timer_create(bm_osal_timer_t* timer, const char* name, uint32_t period_ms,
                          bool auto_reload, bm_osal_timer_callback_t callback, void* arg) {
    (void)name;
    (void)period_ms;
    (void)auto_reload;
    (void)callback;
    (void)arg;
    *timer = BM_OSAL_DUMMY_TIMER_HANDLE;
    return true;
}

bool bm_osal_timer_start(bm_osal_timer_t timer) {
    (void)timer;
    return true;
}

bool bm_osal_timer_stop(bm_osal_timer_t timer) {
    (void)timer;
    return true;
}

bool bm_osal_timer_delete(bm_osal_timer_t timer) {
    (void)timer;
    return true;
}

// ====================================================
// Delay API
// ====================================================

void bm_osal_delay_ms(uint32_t ms) {
    (void)ms;
}

// ====================================================
// Assert API
// ====================================================

void bm_osal_assert(int condition) {
    if (!condition) {
        // In test, we could optionally log or abort here
    }
}
