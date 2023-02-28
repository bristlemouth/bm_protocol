#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "bsp.h"
#include "io.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef bool (*micCallbackFn)(const uint32_t *buff, uint32_t numSamples, void *args);

bool micInit(SAI_HandleTypeDef *saiHandle, IOPinHandle_t *powerPin);
bool micSample(uint32_t durationSec, micCallbackFn callback, void *args);
float micGetDB(const uint32_t *samples, uint32_t numSamples);

/// Use this (default) task notification index for general purpose use
#define MIC_TASK_NOITIFY_INDEX 0

/// This task notification index is used exclusively for I2S microphone events
#define MIC_I2S_NOTIFY_INDEX 1

#ifdef __cplusplus
}
#endif
