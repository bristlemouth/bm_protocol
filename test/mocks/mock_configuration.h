#pragma once

#include "abstract_configuration.h"
#include "gmock/gmock.h"

class MockConfiguration : public cfg::AbstractConfiguration {
public:
  MOCK_METHOD(bool, getConfigCbor,
              (const char *key, size_t key_len, uint8_t *value, size_t &value_len));
  MOCK_METHOD(bool, setConfigCbor,
              (const char *key, size_t key_len, uint8_t *value, size_t value_len));
};
