#pragma once

#include <inttypes.h>
#include <string.h>

namespace cfg {

// Allows for mocking in unit tests
class AbstractConfiguration {
public:
  virtual bool getConfigCbor(const char *key, size_t key_len, uint8_t *value,
                             size_t &value_len) = 0;
  virtual bool setConfigCbor(const char *key, size_t key_len, uint8_t *value,
                             size_t value_len) = 0;
};

} // namespace cfg
