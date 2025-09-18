/**
 * @file simple_bridgeLog_stub.cpp
 * @brief Simple stub for bridgeLog functions for integration tests
 */

#include "bridgeLog.h"

// Simple stub implementation that does nothing
void bridgeLogPrint(bridgeLogType_e type, BmLogLevel level, bool use_header, const char* format, ...) {
    // Suppress unused parameter warnings
    (void)type;
    (void)level;
    (void)use_header;
    (void)format;

    // Do nothing - just a stub for testing
}
