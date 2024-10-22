#pragma once

#include "nau7802.h"

#define DEFAULT_CALIBRATION_FACTOR 800.0
#define DEFAULT_ZERO_OFFSET 80000

typedef struct {
  float calibration_factor;
  int32_t zero_offset;
} LoadCellConfig_t;

void loadCellSamplerInit(NAU7802 *sensor, LoadCellConfig_t cfg);
