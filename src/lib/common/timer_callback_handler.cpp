extern "C" {
#include "timer_callback_handler.h"
}
#include "FreeRTOS.h"
#include "task.h"
#include "task_priorities.h"
#include "queue.h"

#define TIMER_CB_QUEUE_LEN (10)

typedef struct timer_callback_handler_ctx_t {
    QueueHandle_t cb_queue;
} timer_callback_handler_ctx_t;

typedef struct timer_callback_handler_event_t {
    timer_handler_cb cb;
    void* arg;
} timer_callback_handler_event_t;

static timer_callback_handler_ctx_t _ctx;

static void timer_callback_handler_task(void* arg) {
    (void) arg;
    timer_callback_handler_event_t evt;
    while (true) {
        if(xQueueReceive(_ctx.cb_queue, &evt, portMAX_DELAY) == pdTRUE){
            if(evt.cb) {
                evt.cb(evt.arg);
            }
        }
    }
}

void timer_callback_handler_init() {
    _ctx.cb_queue = xQueueCreate(TIMER_CB_QUEUE_LEN, sizeof(timer_callback_handler_event_t));
    configASSERT(_ctx.cb_queue);
    configASSERT(xTaskCreate(timer_callback_handler_task,
                                "timer_cb_handler",
                                  1024,
                                  NULL,
                                  // Start with very high priority during boot then downgrade
                                  // once done initializing everything
                                  TIMER_HANDLER_TASK_PRIORITY,
                                  NULL) == pdTRUE);
}

bool timer_callback_handler_send_cb(timer_handler_cb cb, void* arg, uint32_t timeoutMs) {
    timer_callback_handler_event_t evt = {
        .cb = cb,
        .arg = arg,
    };
    return (xQueueSend(_ctx.cb_queue, &evt, pdMS_TO_TICKS(timeoutMs)) == pdTRUE);
}
