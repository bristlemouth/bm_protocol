#pragma once
#include <stdlib.h>

void bridgeLogPrintf(const char *str, size_t len);

#define BRIDGE_LOG_PRINT(x)  bridgeLogPrintf(x, sizeof(x))
#define BRIDGE_LOG_PRINTN(x,n)  bridgeLogPrintf(x, n)
