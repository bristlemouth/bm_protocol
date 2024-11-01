#pragma once
#include "bm_configs_generic.h"
#include "cbor.h"
#include "messages.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CONFIG_START_OFFSET_IN_BYTES 0
#define CONFIG_LOAD_TIMEOUT_MS 5000

#define MAX_NUM_KV 50
#define MAX_KEY_LEN_BYTES 32
#define MAX_STR_LEN_BYTES 50
#define MAX_CONFIG_BUFFER_SIZE_BYTES 50
#define CONFIG_VERSION 0 // FIXME: Put this in the default config file.

typedef enum {
  UINT32,
  INT32,
  FLOAT,
  STR,
  BYTES,
  ARRAY,
} ConfigDataTypes;

typedef struct {
  uint32_t crc32;
  uint32_t version;
  uint8_t numKeys;
} __attribute__((packed, aligned(1))) ConfigPartitionHeader;

typedef struct {
  char keyBuffer[MAX_KEY_LEN_BYTES];
  size_t keyLen;
  ConfigDataTypes valueType;
} __attribute__((packed, aligned(1))) ConfigKey;

typedef struct {
  uint8_t valueBuffer[MAX_CONFIG_BUFFER_SIZE_BYTES];
} __attribute__((packed, aligned(1))) ConfigValue;

typedef struct {
  ConfigPartitionHeader header;
  ConfigKey keys[MAX_NUM_KV];
  ConfigValue values[MAX_NUM_KV];
} __attribute__((packed, aligned(1))) ConfigPartition;

void config_init(void);
bool get_config_uint(BmConfigPartition partition, const char *key, size_t key_len,
                     uint32_t *value);
bool get_config_int(BmConfigPartition partition, const char *key, size_t key_len,
                    int32_t *value);
bool get_config_float(BmConfigPartition partition, const char *key, size_t key_len,
                      float *value);
bool get_config_string(BmConfigPartition partition, const char *key, size_t key_len,
                       char *value, size_t *value_len);
bool get_config_buffer(BmConfigPartition partition, const char *key, size_t key_len,
                       uint8_t *value, size_t *value_len);
bool get_config_cbor(BmConfigPartition partition, const char *key, size_t key_len,
                     uint8_t *value, size_t *value_len);
bool set_config_uint(BmConfigPartition partition, const char *key, size_t key_len,
                     uint32_t value);
bool set_config_int(BmConfigPartition partition, const char *key, size_t key_len,
                    int32_t value);
bool set_config_float(BmConfigPartition partition, const char *key, size_t key_len,
                      float value);
bool set_config_string(BmConfigPartition partition, const char *key, size_t key_len,
                       const char *value, size_t value_len);
bool set_config_buffer(BmConfigPartition partition, const char *key, size_t key_len,
                       const uint8_t *value, size_t value_len);
bool set_config_cbor(BmConfigPartition partition, const char *key, size_t key_len,
                     uint8_t *value, size_t value_len);
const ConfigKey *get_stored_keys(BmConfigPartition partition, uint8_t *num_stored_keys);
bool remove_key(BmConfigPartition partition, const char *key, size_t key_len);
const char *data_type_enum_to_str(ConfigDataTypes type);
bool save_config(BmConfigPartition partition, bool restart);
bool get_value_size(BmConfigPartition partition, const char *key, size_t key_len, size_t *size);
bool needs_commit(BmConfigPartition partition);
bool cbor_type_to_config(const CborValue *value, ConfigDataTypes *configType);

#ifdef __cplusplus
}
#endif
