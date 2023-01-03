#include "FreeRTOS.h"

void * operator new(size_t size) {
  void *buff = pvPortMalloc(size);
  configASSERT(buff != NULL);
  return buff;
}

void * operator new[](size_t size) {
  void *buff = pvPortMalloc(size);
  configASSERT(buff != NULL);
  return buff;
}

void operator delete(void * ptr) {
  vPortFree(ptr);
}

void operator delete[](void * ptr) {
  vPortFree(ptr);
}
void operator delete(void * ptr, unsigned int x) {
  (void) x;
  vPortFree(ptr);
}

void operator delete[](void * ptr, unsigned int x) {
  (void) x;
  vPortFree(ptr);
}
