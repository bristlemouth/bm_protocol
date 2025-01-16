#pragma once

#include "FreeRTOS.h"
#include "math.h"
#include "timers.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  uint16_t year;
  uint8_t month;
  uint8_t day;
  uint8_t hour;
  uint8_t min;
  uint8_t sec;
  uint32_t usec;
} utcDateTime_t;

bool bStrtof(char *numStr, float *fVal);
bool bStrtod(char *numStr, double *dVal);
uint32_t utcFromDateTime(uint16_t year, uint8_t month, uint8_t day, uint8_t hour,
                         uint8_t minute, uint8_t second);
void dateTimeFromUtc(uint64_t utc_us, utcDateTime_t *dateTime);
uint32_t timeRemainingGeneric(uint32_t startTime, uint32_t currentTime, uint32_t timeout);

char *duplicateStr(const char *inStr);
bool isASCIIString(const char *str);
double degToRad(double deg);

#define timeRemainingTicks(startTicks, timeoutTicks)                                           \
  timeRemainingGeneric(startTicks, xTaskGetTickCount(), timeoutTicks)
#define timeRemainingTicksFromISR(startTicks, timeoutTicks)                                    \
  timeRemainingGeneric(startTicks, xTaskGetTickCountFromISR(), timeoutTicks)
#define timeRemainingMs(startTimeMs, timeoutMs)                                                \
  pdTICKS_TO_MS(timeRemainingGeneric(pdMS_TO_TICKS(startTimeMs), xTaskGetTickCount(),          \
                                     pdMS_TO_TICKS(timeoutMs)))
#define timeRemainingMsFromISR(startTimeMs, timeoutMs)                                         \
  pdTICKS_TO_MS(timeRemainingGeneric(pdMS_TO_TICKS(startTimeMs), xTaskGetTickCountFromISR(),   \
                                     pdMS_TO_TICKS(timeoutMs)))

// Handle float/double to integer conversion nicely, without this, there will
// be off-by-one errors.
#define F2INT(val) ((val) >= 0 ? (int32_t)((val) + 0.5) : (int32_t)((val) - 0.5))
#define D2INT F2INT

#define F2UINT(val) ((uint32_t)((val) + 0.5))
#define D2UINT F2UINT

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

#ifdef __GNUC__
#define UNUSED_FUNCTION(x) __attribute__((__unused__)) UNUSED_##x
#else
#define UNUSED_FUNCTION(x) UNUSED_##x
#endif

#ifdef __cplusplus
}
#endif
