#pragma once
#include "abstract_configuration.h"
#include "nvmPartition.h"
#include "cbor.h"
#include "messages.h"
#include "bm_configs_generic.h"

static constexpr uint8_t MAX_NUM_KV                         = 50;
static constexpr uint8_t MAX_KEY_LEN_BYTES                  = 32;
static constexpr uint8_t MAX_STR_LEN_BYTES                  = 50;
static constexpr uint8_t MAX_CONFIG_BUFFER_SIZE_BYTES       = 50;
static constexpr uint32_t CONFIG_VERSION        = 0; // FIXME: Put this in the default config file.

typedef struct ConfigPartitionHeader {
    uint32_t crc32;
    uint32_t version;
    uint8_t numKeys;
} __attribute__((packed, aligned(1))) ConfigPartitionHeader_t;

typedef struct ConfigKey {
    char keyBuffer[MAX_KEY_LEN_BYTES];
    size_t keyLen;
    GenericConfigDataTypes valueType;
} __attribute__((packed, aligned(1))) ConfigKey_t;

typedef struct ConfigValue {
    uint8_t valueBuffer[MAX_CONFIG_BUFFER_SIZE_BYTES];
}__attribute__((packed, aligned(1))) ConfigValue_t;

typedef struct ConfigPartition {
    ConfigPartitionHeader_t header;
    ConfigKey_t keys[MAX_NUM_KV];
    ConfigValue_t values[MAX_NUM_KV];
}__attribute__((packed, aligned(1))) ConfigPartition_t;

typedef bool (*ConfigRead)(BmConfigPartition partition, uint32_t offset, uint8_t *buffer, size_t length, uint32_t timeout_ms);
typedef bool (*ConfigWrite)(BmConfigPartition partition, uint32_t offset, uint8_t *buffer, size_t length, uint32_t timeout_ms);
typedef void (*ConfigRestart)(void);

void config_init(ConfigRead read, ConfigWrite write, ConfigRestart restart);
bool get_config_uint(BmConfigPartition partition, const char * key, size_t key_len, uint32_t &value);
bool get_config_int(BmConfigPartition partition, const char * key, size_t key_len, int32_t &value);
bool get_config_float(BmConfigPartition partition, const char * key, size_t key_len, float &value);
bool get_config_string(BmConfigPartition partition, const char * key, size_t key_len, char *value, size_t &value_len);
bool get_config_buffer(BmConfigPartition partition, const char * key, size_t key_len, uint8_t *value, size_t &value_len);
bool get_config_cbor(BmConfigPartition partition, const char * key, size_t key_len, uint8_t *value, size_t &value_len);
bool set_config_uint(BmConfigPartition partition, const char * key, size_t key_len, uint32_t value);
bool set_config_int(BmConfigPartition partition, const char * key, size_t key_len, int32_t value);
bool set_config_float(BmConfigPartition partition, const char * key, size_t key_len, float value);
bool set_config_string(BmConfigPartition partition, const char * key, size_t key_len, const char *value, size_t value_len);
bool set_config_buffer(BmConfigPartition partition, const char * key, size_t key_len, const uint8_t *value, size_t value_len);
bool set_config_cbor(BmConfigPartition partition, const char * key, size_t key_len, uint8_t *value, size_t value_len);
const ConfigKey_t* get_stored_keys(BmConfigPartition partition, uint8_t &num_stored_keys);
bool remove_key(BmConfigPartition partition, const char * key, size_t key_len);
const char* data_type_enum_to_str(GenericConfigDataTypes type);
bool save_config(BmConfigPartition partition, bool restart=true);
bool get_value_size(BmConfigPartition partition, const char * key, size_t key_len, size_t &size);
bool needs_commit(BmConfigPartition partition);
bool cbor_type_to_config(const CborValue *value, GenericConfigDataTypes &configType);

static constexpr uint32_t CONFIG_START_OFFSET_IN_BYTES = 0;
static constexpr uint32_t CONFIG_LOAD_TIMEOUT_MS = 5000;

