
#include "FreeRTOS.h"

static TickType_t _current_ticks = 0;

void vTaskDelay( const TickType_t xTicksToDelay ){
  _current_ticks = _current_ticks + xTicksToDelay;
}

TickType_t xTaskGetTickCount() {
  return _current_ticks;
}

void xTaskSetTickCount(uint32_t xCurrentTickCount) {
  _current_ticks = (TickType_t) xCurrentTickCount;
}
