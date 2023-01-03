#pragma once
#ifdef TRACE_ENABLE

#include <stdint.h>

#ifndef TRACE_FAST
// Use less memory to store events with a slight performance cost due to unaligned access
#define TRACE_SMALL
#endif

#ifdef TRACE_USE_COREDEBUG
// TODO - figure out how to make processor independent
#include "stm32l496xx.h"
#endif

// TODO - allow overriding by application
// NOTE: TRACE_BUFF_LEN MUST be a power of 2!
#ifndef TRACE_BUFF_LEN
#define TRACE_BUFF_LEN (128)
#endif

#define TRACE_BUFF_MASK (TRACE_BUFF_LEN - 1)

// TODO - allow overriding by application ? (Would also need to change decoder)
typedef enum traceEventType {
  kTraceEventUnknown = 0,
  kTraceEventTaskStart,
  kTraceEventTaskStop,
  kTraceEventISRStart,
  kTraceEventISRStop,
  kTraceEventSDWriteStart,
  kTraceEventSDWriteStop,
  kTraceEventSDWriteErr,
  kTraceEventSDReadStart,
  kTraceEventSDReadStop,
  kTraceEventSDReadErr,
  kTraceEventSerialByte,
  kTraceEventMax
} traceEventType_t;

typedef struct {
  uint32_t          timestamp; // Timestamp of trace event
  traceEventType_t  eventType; // Event type
  void              *arg;      // Additional information for trace event
#ifdef TRACE_SMALL
} __attribute__((packed)) traceEvent_t;
#else
} traceEvent_t;
#endif

extern traceEvent_t traceEvents[TRACE_BUFF_LEN];
extern uint32_t traceEventIdx;

#ifdef TRACE_USE_COREDEBUG
// Switch between arm trace counter and tickCount if needed!
inline uint32_t traceGetCount() {
  return DWT->CYCCNT;
}
#else
uint32_t traceGetCount();
#endif

__STATIC_FORCEINLINE void traceAdd(traceEventType_t eventType, void *arg) {
  __disable_irq();
  traceEvents[traceEventIdx].timestamp = traceGetCount();
  traceEvents[traceEventIdx].eventType = eventType;
  traceEvents[traceEventIdx].arg = arg;

  // Move on to next trace event
  traceEventIdx++;

  // wrap around circular buffer
  traceEventIdx &= TRACE_BUFF_MASK;
  __enable_irq();
}

// TODO: Have ISR safe and not-safe definitions
#define traceAddFromISR traceAdd

#ifdef TRACE_TASKS
#define traceTASK_SWITCHED_IN() traceAdd(kTraceEventTaskStart, (void *)pxCurrentTCB)
#define traceTASK_SWITCHED_OUT() traceAdd(kTraceEventTaskStop, (void *)pxCurrentTCB)
#endif

//
// ISR Tracing support
// To use you must declare wrapping functions for each ISR you want to trace
// For example:
// ISR_TRACE_WRAPPER(LPTIM1_IRQHandler);
//
// Then you must use the new tracing function in the vector table
//
// __attribute__ ((section(".isr_vector")))
//   void (* const g_pfnVectors[])(void) = {
//  (void *)&_estack,                         /*     Initial Stack Pointer */
//  Reset_Handler,                            /*     Reset Handler */
// ...
//   ISR_TRACE(LPUART1_IRQHandler),
// ...
// };
//
#ifdef TRACE_ISR

#ifndef TRACE_USE_COREDEBUG
#error MUST use coredebug trace for ISR tracing!
#endif

#define ISR_TRACE(NAME) NAME##_trace

#define ISR_TRACE_WRAPPER(NAME) \
void NAME##_trace() { \
  traceAddFromISR(kTraceEventISRStart, (void *)NAME); \
  NAME(); \
  traceAddFromISR(kTraceEventISRStop, (void *)NAME); \
}
#else
// Make sure things work when TRACE_ISR is disabled
#define ISR_TRACE(NAME) NAME
#endif

#endif // TRACE_ENABLE
