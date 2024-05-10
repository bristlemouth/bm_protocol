#include "trace.h"
#include "FreeRTOS.h"
#include "task.h"

#ifdef TRACE_ENABLE

#if TRACE_BUFF_LEN & (TRACE_BUFF_LEN - 1)
#error TRACE_BUFF_LEN MUST be a power of 2!!!
#endif

traceEvent_t traceEvents[TRACE_BUFF_LEN] __attribute__((section(".noinit")));
uint32_t traceEventIdx = 0;

// Only initialize coredebug if we're using it
#ifdef TRACE_USE_COREDEBUG
// Using __attribute__ ((constructor)) to call before main()
void __attribute__ ((constructor))traceEnable() {
  // Enable ARM's coreDebug clock
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  //
  DWT->CYCCNT = 0;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}
#else
uint32_t traceGetCount() {
  return xTaskGetTickCount();
}
#endif

#endif
