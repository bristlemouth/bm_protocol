#pragma once
#include <stdlib.h>

constexpr size_t SENSOR_LOG_BUF_SIZE = 512;

typedef enum {
  BM_COMMON_IND,
  BM_COMMON_AGG,
} bridgeSensorLogType_e;

typedef enum {
  BRIDGE_SYS,
  BRIDGE_CFG,
} bridgeLogType_e;

void bridgeLogPrintf(const char *str, size_t len, bridgeLogType_e type);
void bridgeSensorLogPrintf(bridgeSensorLogType_e type, const char *str, size_t len);

#define BRIDGE_LOG_PRINT(x) bridgeLogPrintf(x, sizeof(x), BRIDGE_SYS)
#define BRIDGE_LOG_PRINTN(x, n) bridgeLogPrintf(x, n, BRIDGE_SYS)
#define BRIDGE_CFG_LOG_PRINT(x) bridgeLogPrintf(x, sizeof(x), BRIDGE_CFG)
#define BRIDGE_CFG_LOG_PRINTN(x, n) bridgeLogPrintf(x, n, BRIDGE_CFG)
#define BRIDGE_SENSOR_LOG_PRINT(type, x) bridgeSensorLogPrintf(type, x, sizeof(x))
#define BRIDGE_SENSOR_LOG_PRINTN(type, x, n) bridgeSensorLogPrintf(type, x, n)