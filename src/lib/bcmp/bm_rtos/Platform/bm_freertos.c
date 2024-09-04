#include "bm_rtos.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "timers.h"

void *bm_malloc(size_t size) {
    return pvPortMalloc(size);
}

void bm_free(void *ptr) {
    vPortFree(ptr);
}

void bm_delay(uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}

bm_queue_handle_t bm_queue_create(uint32_t queue_length, uint32_t item_size) {
    return xQueueCreate(queue_length, item_size);
}

int bm_queue_receive(bm_queue_handle_t queue, void *item, uint32_t timeout_ms) {
    return xQueueReceive(queue, item, pdMS_TO_TICKS(timeout_ms));
}

int bm_queue_send(bm_queue_handle_t queue, const void *item, uint32_t timeout_ms) {
    return xQueueSend(queue, item, pdMS_TO_TICKS(timeout_ms));
}


