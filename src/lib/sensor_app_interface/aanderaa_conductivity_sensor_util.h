#pragma once
#include <inttypes.h>
#include <stdlib.h>

namespace AanderaaConductivitySensorUtil {

typedef enum DataType {
  CONDUCTIVITY,
  TEMPERATURE,
  SALINITY,
  WATER_DENSITY,
  SOUND_SPEED,
  DEPTH,
} DataType_e;

bool validSensorDataString(const char *s, size_t len);
bool validSensorData(DataType_e type, double val);
void preprocessLine(char *str, uint16_t &len);

} // namespace AanderaaConductivitySensorUtil
