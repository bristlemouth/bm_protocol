#include "serial.h"
#include "bsp.h"
#include "debug_configuration.h"
#include "trace.h"

extern traceEvent_t user_traceEvents[TRACE_BUFF_LEN];
extern uint32_t user_fullTicksLeft;
extern uint32_t user_expectedTicks;
extern uint32_t user_lpuart_start_cpu_cycles;
extern uint32_t user_lpuart_stop_cpu_cycles;

#pragma once

void setup(void);
void loop(void);
