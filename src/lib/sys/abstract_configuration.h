#pragma once

#include "configuration.h"
#include <inttypes.h>
#include <string.h>

// Allows for mocking in unit tests
class AbstractConfiguration {
public:
  virtual bool get_config_cbor(BmConfigPartition partition, const char *key, size_t key_len,
                               uint8_t *value, size_t *value_len) = 0;
  virtual bool set_config_cbor(BmConfigPartition partition, const char *key, size_t key_len,
                               uint8_t *value, size_t value_len) = 0;
};
