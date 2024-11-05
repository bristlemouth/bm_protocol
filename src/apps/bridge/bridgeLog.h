#pragma once
#include "bm_common_structs.h"
#include <stdarg.h>
#include <stdlib.h>

constexpr size_t SENSOR_LOG_BUF_SIZE = 512;
constexpr bool USE_HEADER = true;
constexpr bool NO_HEADER = false;

typedef enum {
  BM_COMMON_IND,
  BM_COMMON_AGG,
} bridgeSensorLogType_e;

typedef enum {
  BRIDGE_SYS,
  BRIDGE_CFG,
} bridgeLogType_e;

void bridgeLogPrint(bridgeLogType_e type, BmLogLevel level, bool print_header,
                    const char *format, ...);
void vBridgeLogPrint(bridgeLogType_e type, BmLogLevel level, bool print_header,
                     const char *format, va_list va_args);
void bridgeSensorLogPrintf(bridgeSensorLogType_e type, const char *str, size_t len);
#define BRIDGE_SENSOR_LOG_PRINT(type, x) bridgeSensorLogPrintf(type, x, sizeof(x))
#define BRIDGE_SENSOR_LOG_PRINTN(type, x, n) bridgeSensorLogPrintf(type, x, n)
