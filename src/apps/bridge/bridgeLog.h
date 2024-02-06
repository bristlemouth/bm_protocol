#pragma once
#include <stdlib.h>

constexpr size_t SENSOR_LOG_BUF_SIZE = 512;

typedef enum {
    BM_COMMON_IND,
    BM_COMMON_AGG,
} bridgeSensorLogType_e;

void bridgeLogPrintf(const char *str, size_t len);
void bridgeSensorLogPrintf(bridgeSensorLogType_e type, const char *str, size_t len);

#define BRIDGE_LOG_PRINT(x)  bridgeLogPrintf(x, sizeof(x))
#define BRIDGE_LOG_PRINTN(x,n)  bridgeLogPrintf(x, n)
#define BRIDGE_SENSOR_LOG_PRINT(type, x)  bridgeSensorLogPrintf(type, x, sizeof(x))
#define BRIDGE_SENSOR_LOG_PRINTN(type, x,n)  bridgeSensorLogPrintf(type, x, n)