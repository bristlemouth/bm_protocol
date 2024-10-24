#pragma once
#include "abstract_configuration.h"
#include "nvmPartition.h"
#include "cbor.h"

namespace cfg {

static constexpr uint8_t MAX_NUM_KV                         = 50;
static constexpr uint8_t MAX_KEY_LEN_BYTES                  = 32;
static constexpr uint8_t MAX_STR_LEN_BYTES                  = 50;
static constexpr uint8_t MAX_CONFIG_BUFFER_SIZE_BYTES       = 50;
static constexpr uint32_t CONFIG_VERSION        = 0; // FIXME: Put this in the default config file.

typedef enum ConfigDataTypes{
    UINT32,
    INT32,
    FLOAT,
    STR,
    BYTES,
    ARRAY,
} ConfigDataTypes_e;

typedef struct ConfigPartitionHeader {
    uint32_t crc32;
    uint32_t version;
    uint8_t numKeys;
} __attribute__((packed, aligned(1))) ConfigPartitionHeader_t;

typedef struct ConfigKey {
    char keyBuffer[MAX_KEY_LEN_BYTES];
    size_t keyLen;
    ConfigDataTypes_e valueType;
} __attribute__((packed, aligned(1))) ConfigKey_t;

typedef struct ConfigValue {
    uint8_t valueBuffer[MAX_CONFIG_BUFFER_SIZE_BYTES];
}__attribute__((packed, aligned(1))) ConfigValue_t;

typedef struct ConfigPartition {
    ConfigPartitionHeader_t header;
    ConfigKey_t keys[MAX_NUM_KV];
    ConfigValue_t values[MAX_NUM_KV];
}__attribute__((packed, aligned(1))) ConfigPartition_t;

class Configuration : public AbstractConfiguration {
public:
    Configuration(NvmPartition &flash_partition, uint8_t *ram_partition, size_t ram_partition_size);
    bool getConfig(const char * key, size_t key_len, uint32_t &value);
    bool getConfig(const char * key, size_t key_len, int32_t &value);
    bool getConfig(const char * key, size_t key_len, float &value);
    bool getConfig(const char * key, size_t key_len, char *value, size_t &value_len);
    bool getConfig(const char * key, size_t key_len, uint8_t *value, size_t &value_len);
    bool getConfigCbor(const char * key, size_t key_len, uint8_t *value, size_t &value_len);
    bool setConfig(const char * key, size_t key_len, uint32_t value);
    bool setConfig(const char * key, size_t key_len, int32_t value);
    bool setConfig(const char * key, size_t key_len, float value);
    bool setConfig(const char * key, size_t key_len, const char *value, size_t value_len);
    bool setConfig(const char * key, size_t key_len, const uint8_t *value, size_t value_len);
    const ConfigKey_t* getStoredKeys(uint8_t &num_stored_keys);
    bool setConfigCbor(const char * key, size_t key_len, uint8_t *value, size_t value_len);
    bool removeKey(const char * key, size_t key_len);
    static const char* dataTypeEnumToStr(ConfigDataTypes_e type);
    bool saveConfig(bool restart=true);
    bool getValueSize(const char * key, size_t key_len, size_t &size);
    bool needsCommit(void);
    static bool cborTypeToConfigType(const CborValue *value, ConfigDataTypes_e &configType);
private:
    bool findKeyIndex(const char * key, size_t len, uint8_t &idx);
    bool prepareCborParser(const char * key, size_t key_len, CborValue &it, CborParser &parser);
    bool prepareCborEncoder(const char * key, size_t key_len, CborEncoder &encoder, uint8_t &keyIdx, bool &keyExists);
    bool loadAndVerifyNvmConfig(void);

    static constexpr uint32_t CONFIG_START_OFFSET_IN_BYTES = 0;
    static constexpr uint32_t CONFIG_LOAD_TIMEOUT_MS = 5000;

private:
    NvmPartition &_flash_partition;
    size_t _ram_partition_size;
    ConfigPartition_t* _ram_partition;
    bool _needs_commit;
};
} // namespace cfg
