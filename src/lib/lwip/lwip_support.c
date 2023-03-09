#include "FreeRTOS.h"

void lwip_example_app_platform_assert(const char *msg, int line, const char *file) {
  (void)msg;
  (void)line;
  (void)file;
  configASSERT(0);
}

uint32_t
lwip_port_rand(void)
{
  // TODO: Use random number generator 😅
  return (uint32_t)7;
}
