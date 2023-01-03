
#include "FreeRTOS.h"

void vTaskDelay( const TickType_t xTicksToDelay ){
  (void)xTicksToDelay;
}

TickType_t xTaskGetTickCount() {
  return 0;
}
