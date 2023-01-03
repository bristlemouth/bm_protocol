#pragma once

#include "printf.h"
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

// Template for debug putchar
typedef void (*DebugPutc_t)(char, void*);

void debugInit(DebugPutc_t fn);
void swoPutchar(char character, void* arg);
void debugPrintBuff(void *buff, size_t len);

/*
  Used when printing debug data that should be compiled out in release builds.
  No header stub declared for printf as it exists already in newlib's stdio.h,
  but is overriden by us in the debug.c implementation file. Use of do/while loop
  guards against unexpected behavior when debug_printf macro surrounded by other
  if/else statements. See https://stackoverflow.com/a/1644898 for (lots) more detail.
*/
#define debug_printf(format, ...)                     \
    do {                                              \
      if (BUILD_DEBUG) {                              \
        printf(format, ##__VA_ARGS__);                \
      }                                               \
    } while (0)

#define panic_printf( ... ) fctprintf(swoPutchar, NULL, __VA_ARGS__)

#ifdef __cplusplus
}
#endif
