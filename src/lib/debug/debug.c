#include <string.h>

#include "debug.h"
#include "bsp.h"
#include "FreeRTOS.h"
#include "semphr.h"

static DebugPutc_t debugPutcFn = NULL;
static SemaphoreHandle_t debugPrintMutex;

/*
  Override newlib printf implementation with our own internal one. This will send
  characters over the USB virtual COM port.
*/
int printf(const char* format, ...) {
	if(debugPutcFn == NULL){
    return -1;
  }

	configASSERT(xSemaphoreTake(debugPrintMutex, portMAX_DELAY) == pdTRUE);
	va_list va;
  va_start(va, format);
	int rval = vfctprintf(debugPutcFn, NULL, format, va);
	va_end(va);
	xSemaphoreGive(debugPrintMutex);

	return rval;
}

void debugInit(DebugPutc_t fn) {
  debugPutcFn = fn;
  debugPrintMutex = xSemaphoreCreateMutex();
  configASSERT(debugPrintMutex);
}

void swoPutchar(char character, void* arg) {
  (void)arg;
  ITM_SendChar(character);
}

void debugPrintBuff(void *buff, size_t len) {
  configASSERT(buff != NULL);
  uint8_t *bytes = (uint8_t *)buff;
  while(len-- > 0) {
    printf("%02X ", *bytes++);
  }
  printf("\n");
}
