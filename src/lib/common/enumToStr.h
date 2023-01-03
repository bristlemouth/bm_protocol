#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  uint32_t val;
  const char *str;
} enumStrLUT_t;

const char *enumToStr(const enumStrLUT_t *lut, uint32_t val);

#ifdef __cplusplus
}
#endif
