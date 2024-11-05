#pragma once

#include "abstract_configuration.h"
#include "configuration.h"
#include "gmock/gmock.h"

class MockConfiguration : public AbstractConfiguration {
public:
  MOCK_METHOD(bool, get_config_cbor,
              (BmConfigPartition partition, const char *key, size_t key_len, uint8_t *value,
               size_t *value_len));
  MOCK_METHOD(bool, set_config_cbor,
              (BmConfigPartition partition, const char *key, size_t key_len, uint8_t *value,
               size_t value_len));
};
