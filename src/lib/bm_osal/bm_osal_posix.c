/**
 * @file bm_osal_mock.c
 * @brief Mock POSIX Implementation of Bristlemouth OS Abstraction Layer.
 *
 * This file provides a pthread-based mock version of bm_osal for unit testing
 * on POSIX systems using Google Test or similar frameworks.
 */

#include "bm_osal.h"
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

const bm_osal_result_t BM_OSAL_TRUE = 1;
const bm_osal_result_t BM_OSAL_FALSE = 0;

// ====================================================
// Mutex API
// ====================================================

bool bm_osal_mutex_create(bm_osal_mutex_t* mutex) {
    pthread_mutex_t* m = malloc(sizeof(pthread_mutex_t));
    if (!m) return false;
    pthread_mutex_init(m, NULL);
    *mutex = (bm_osal_mutex_t)m;
    return true;
}

bool bm_osal_mutex_lock(bm_osal_mutex_t mutex, uint32_t timeout_ms) {
    (void)timeout_ms; // For simplicity, ignore timeout
    return pthread_mutex_lock((pthread_mutex_t*)mutex) == 0;
}

bool bm_osal_mutex_unlock(bm_osal_mutex_t mutex) {
    return pthread_mutex_unlock((pthread_mutex_t*)mutex) == 0;
}

bool bm_osal_mutex_delete(bm_osal_mutex_t mutex) {
    pthread_mutex_destroy((pthread_mutex_t*)mutex);
    free(mutex);
    return true;
}

// ====================================================
// Delay API
// ====================================================

void bm_osal_delay_ms(uint32_t ms) {
    usleep(ms * 1000);
}

// ====================================================
// Assert API
// ====================================================

void bm_osal_assert(int condition) {
    assert(condition);
}
