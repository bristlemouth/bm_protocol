#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

bool flashErase(uint32_t addr, size_t len);
bool flashWrite(uint32_t addr, const uint8_t * data, uint32_t len);

#ifdef __cplusplus
}
#endif
