#pragma once
#include <inttypes.h>
#include <stdlib.h>

namespace BmRbrSensorUtil {
typedef enum DataType {
  TEMPERATURE,
  PRESSURE,
} DataType_e;

bool validSensorDataString(const char *s, size_t strlen);
bool validSensorData(DataType_e type, double val);
bool validSensorOutputformat(const char *s, size_t strlen);
void preprocessLine(char *str, uint16_t &len);
} // namespace BmRbrSensorUtil