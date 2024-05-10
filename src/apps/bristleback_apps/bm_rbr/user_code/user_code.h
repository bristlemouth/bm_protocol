#include "serial.h"
#include "bsp.h"
#include "debug_configuration.h"
#include "trace.h"

extern traceEvent_t user_traceEvents[TRACE_BUFF_LEN];

#pragma once

void setup(void);
void loop(void);
