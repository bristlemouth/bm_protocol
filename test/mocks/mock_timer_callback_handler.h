#pragma once
#include "fff.h"
#include "timer_callback_handler.h"

DECLARE_FAKE_VALUE_FUNC(bool, timer_callback_handler_send_cb,timer_handler_cb , void* , uint32_t );
