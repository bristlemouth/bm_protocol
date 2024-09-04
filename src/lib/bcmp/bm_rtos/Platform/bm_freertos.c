#include "FreeRTOS.h"
#include "bm_rtos.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"
#include "timers.h"

void *bm_malloc(size_t size) { return pvPortMalloc(size); }

void bm_free(void *ptr) { vPortFree(ptr); }

BmQueue bm_queue_create(uint32_t queue_length, uint32_t item_size) {
  return xQueueCreate(queue_length, item_size);
}

BmError bm_queue_receive(BmQueue queue, void *item, uint32_t timeout_ms) {
  if (xQueueReceive(queue, item, pdMS_TO_TICKS(timeout_ms)) == pdPASS) {
    return BM_SUCCESS;
  } else {
    return BM_FAIL;
  }
}

BmError bm_queue_send(BmQueue queue, const void *item, uint32_t timeout_ms) {
  if (xQueueSend(queue, item, pdMS_TO_TICKS(timeout_ms)) == pdPASS) {
    return BM_SUCCESS;
  } else {
    return BM_FAIL;
  }
}

BmSemaphore bm_semaphore_create(void) { return xSemaphoreCreateMutex(); }

BmError bm_semaphore_give(BmSemaphore semaphore) {
  if (xSemaphoreGive(semaphore) == pdPASS) {
    return BM_SUCCESS;
  } else {
    return BM_FAIL;
  }
}

BmError bm_task_create(void (*task)(void *), const char *name, uint32_t stack_size, void *arg,
                       uint32_t priority, void *task_handle) {
  if (xTaskCreate(task, name, stack_size, arg, priority, task_handle) == pdPASS) {
    return BM_SUCCESS;
  } else {
    return BM_FAIL;
  }
}

void bm_start_scheduler(void) {
  vTaskStartScheduler();
}

BmTimer bm_timer_create(void (*callback)(void *), const char *name, uint32_t period_ms,
                        void *arg) {
  return xTimerCreate(name, pdMS_TO_TICKS(period_ms), pdTRUE, arg,
                      (TimerCallbackFunction_t)callback);
}

BmError bm_timer_start(BmTimer timer, uint32_t timeout_ms) {
  if (xTimerStart(timer, pdMS_TO_TICKS(timeout_ms)) == pdPASS) {
    return BM_SUCCESS;
  } else {
    return BM_FAIL;
  }
}

BmError bm_timer_stop(BmTimer timer, uint32_t timeout_ms) {
  if (xTimerStop(timer, pdMS_TO_TICKS(timeout_ms)) == pdPASS) {
    return BM_SUCCESS;
  } else {
    return BM_FAIL;
  }
}

BmError bm_timer_change_period(BmTimer timer, uint32_t period_ms, uint32_t timeout_ms) {
  if (xTimerChangePeriod(timer, pdMS_TO_TICKS(period_ms), pdMS_TO_TICKS(timeout_ms)) ==
      pdPASS) {
    return BM_SUCCESS;
  } else {
    return BM_FAIL;
  }
}

uint32_t bm_get_tick_count(void) { return xTaskGetTickCount(); }

uint32_t bm_ms_to_ticks(uint32_t ms) { return pdMS_TO_TICKS(ms); }

uint32_t bm_ticks_to_ms(uint32_t ticks) { return pdTICKS_TO_MS(ticks); }

void bm_delay(uint32_t ms) { vTaskDelay(pdMS_TO_TICKS(ms)); }