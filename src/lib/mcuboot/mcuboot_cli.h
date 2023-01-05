#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	kMCUBootUpdateRetry = 0,
	kMCUBootUpdateSuccess,
	kMCUBootUpdateNoRetry,
}MCUBootUpdateResult_t;

void mcubootCliInit();
MCUBootUpdateResult_t mcubootFlashFromFile(const char *filename, const size_t bufferSize, bool permanent);

#ifdef __cplusplus
}
#endif
