#pragma once
#include <stdint.h>

typedef void (*timer_handler_cb)(void * arg);

void timer_callback_handler_init();
bool timer_callback_handler_send_cb(timer_handler_cb cb, void* arg, uint32_t timeoutMs);
