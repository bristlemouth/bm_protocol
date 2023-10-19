#include "configuration.h"
#include "FreeRTOS.h"
#include <stdio.h>
#include "crc.h"
#include "reset_reason.h"
#ifndef CBOR_CUSTOM_ALLOC_INCLUDE
#error "CBOR_CUSTOM_ALLOC_INCLUDE must be defined!"
#endif // CBOR_CUSTOM_ALLOC_INCLUDE
#ifndef CBOR_PARSER_MAX_RECURSIONS
#error "CBOR_PARSER_MAX_RECURSIONS must be defined!"
#endif // CBOR_PARSER_MAX_RECURSIONS
namespace cfg {

Configuration::Configuration(NvmPartition& flash_partition, uint8_t *ram_partition, size_t ram_partition_size):_flash_partition(flash_partition), _ram_partition_size(ram_partition_size), _needs_commit(false) {
    configASSERT(ram_partition);
    configASSERT(_ram_partition_size >= sizeof(ConfigPartition_t));
    _ram_partition = reinterpret_cast<ConfigPartition_t*>(ram_partition);
    if(!loadAndVerifyNvmConfig()) {
        printf("Unable to load configs from flash.");
        _ram_partition->header.numKeys = 0;
        _ram_partition->header.version = CONFIG_VERSION;
        // TODO: Once we have default configs, load these into flash.
    } else {
        printf("Succesfully loaded configs from flash.");
    }
}

bool Configuration::loadAndVerifyNvmConfig(void) {
    bool rval = false;
    do {
        if(!_flash_partition.read(CONFIG_START_OFFSET_IN_BYTES, reinterpret_cast<uint8_t *>(_ram_partition), sizeof(ConfigPartition_t), CONFIG_LOAD_TIMEOUT_MS)){
            break;
        }
        uint32_t computed_crc32 = crc32_ieee(reinterpret_cast<const uint8_t *>(&_ram_partition->header.version), (sizeof(ConfigPartition_t)-sizeof(_ram_partition->header.crc32)));
        if(_ram_partition->header.crc32 != computed_crc32) {
            break;
        }
        rval = true;
    } while(0);
    return rval;
}

uint32_t Configuration::getCRC32(void) {
    return _ram_partition->header.crc32;
}

bool Configuration::prepareCborParser(const char * key, size_t key_len, CborValue &it, CborParser &parser) {
    configASSERT(key);
    bool retval = false;
    uint8_t keyIdx;
    do {
        if(key_len > MAX_KEY_LEN_BYTES) {
            break;
        }
        if(!findKeyIndex(key, key_len, keyIdx)){
            break;
        }
        if(cbor_parser_init(_ram_partition->values[keyIdx].valueBuffer, sizeof(_ram_partition->values[keyIdx].valueBuffer), 0, &parser, &it) != CborNoError){
            break;
        }
        retval = true;
    } while(0);
    return retval;
}


/*!
* Get uint64 config
* \param key[in] - null terminated key
* \param value[out] - value
* \returns - true if success, false otherwise.
*/
bool Configuration::getConfig(const char * key, size_t key_len, uint32_t &value) {
    configASSERT(key);
    bool rval = false;

    CborValue it;
    CborParser parser;
    do {
        if(!prepareCborParser(key, key_len, it, parser)){
            break;
        }
        uint64_t temp;
        if(!cbor_value_is_unsigned_integer(&it)){
            break;
        }
        if(cbor_value_get_uint64(&it,&temp) != CborNoError){
            break;
        }
        value = static_cast<uint32_t>(temp);
        rval = true;
    } while(0);
    return rval;
}

/*!
* Get int32 config
* \param key[in] - null terminated key
* \param value[out] - value
* \returns - true if success, false otherwise.
*/
bool Configuration::getConfig(const char * key, size_t key_len, int32_t &value) {
    configASSERT(key);
    bool rval = false;

    CborValue it;
    CborParser parser;
    do {
        if(!prepareCborParser(key, key_len, it, parser)){
            break;
        }
        int64_t temp;
        if(!cbor_value_is_integer(&it)){
            break;
        }
        if(cbor_value_get_int64(&it,&temp) != CborNoError){
            break;
        }
        value = static_cast<int32_t>(temp);
        rval = true;
    } while(0);
    return rval;
}

/*!
* Get float config
* \param key[in] - null terminated key
* \param value[out] - value
* \returns - true if success, false otherwise.
*/
bool Configuration::getConfig(const char * key, size_t key_len, float &value){
    configASSERT(key);
    bool rval = false;

    CborValue it;
    CborParser parser;
    do {
        if(!prepareCborParser(key, key_len, it, parser)){
            break;
        }
        float temp;
        if(!cbor_value_is_float(&it)){
            break;
        }
        if(cbor_value_get_float(&it,&temp) != CborNoError){
            break;
        }
        value = temp;
        rval = true;
    } while(0);
    return rval;
}

/*!
* Get str config
* \param key[in] - null terminated key
* \param value[out] - value (non null terminated)
* \param value_len[in/out] - in: buffer size, out:bytes len
* \returns - true if success, false otherwise.
*/
bool Configuration::getConfig(const char * key, size_t key_len, char *value, size_t &value_len) {
    configASSERT(key);
    configASSERT(value);
    bool rval = false;

    CborValue it;
    CborParser parser;
    do {
        if(!prepareCborParser(key,key_len, it, parser)){
            break;
        }
        if(!cbor_value_is_text_string(&it)){
            break;
        }
        if(cbor_value_copy_text_string(&it,value, &value_len, NULL) != CborNoError){
            break;
        }
        rval = true;
    } while(0);
    return rval;
}

/*!
* Get bytes config
* \param key[in] - null terminated key
* \param value[out] - value
* \param value_len[in/out] - in: buffer size, out:bytes len
* \returns - true if success, false otherwise.
*/
bool Configuration::getConfig(const char * key, size_t key_len, uint8_t *value, size_t &value_len) {
    configASSERT(key);
    configASSERT(value);
    bool rval = false;

    CborValue it;
    CborParser parser;
    do {
        if(!prepareCborParser(key,key_len, it, parser)){
            break;
        }
        if(!cbor_value_is_byte_string(&it)){
            break;
        }
        if(cbor_value_copy_byte_string(&it,value, &value_len, NULL) != CborNoError){
            break;
        }
        rval = true;
    } while(0);
    return rval;
}

/*!
* Get the cbor encoded buffer for a given key.
* \param key[in] - null terminated key
* \param value[out] - value buffer
* \param value_len[in/out] -  in: buffer size, out :buffer len
* \returns - true if success, false otherwise.
*/
bool Configuration::getConfigCbor(const char * key, size_t key_len, uint8_t *value, size_t &value_len) {
    configASSERT(key);
    configASSERT(value);
    bool rval = false;

    CborValue it;
    CborParser parser;
    uint8_t keyIdx;
    do {
        if(!prepareCborParser(key,key_len, it, parser)){
            break;
        }
        if(!cbor_value_is_valid(&it)){
            break;
        }
        if(!findKeyIndex(key, key_len, keyIdx)){
            break;
        }
        size_t buffer_size = sizeof(_ram_partition->values[keyIdx].valueBuffer);
        if(value_len < buffer_size || value_len == 0){
            break;
        }
        memcpy(value, _ram_partition->values[keyIdx].valueBuffer, buffer_size);
        value_len = buffer_size;
        rval = true;
    } while(0);
    return rval;
}

/*! \brief Encode the entire config partition as a CBOR map.
    \param[out] buffer The destination for the encoded CBOR bytes.
    \param[in,out] buffer_len in: The length in bytes of the given buffer.
                              out: The length in bytes of the encoded CBOR.
    \return True on success, false on failure.
 */
bool Configuration::asCborMap(uint8_t *buffer, size_t &buffer_len) {
  configASSERT(buffer);
  bool success = false;
  size_t original_len = buffer_len;

  uint32_t tmpU;
  int32_t tmpI;
  float tmpF;

  char tmpS[MAX_STR_LEN_BYTES];
  size_t tmpSSize = MAX_STR_LEN_BYTES;

  uint8_t tmpB[MAX_STR_LEN_BYTES];
  size_t tmpBSize = MAX_STR_LEN_BYTES;

  CborEncoder encoder;
  CborEncoder map;
  CborError err;
  cbor_encoder_init(&encoder, buffer, buffer_len, 0);

  do {
    err = cbor_encoder_create_map(&encoder, &map, _ram_partition->header.numKeys);
    if (err != CborNoError && err != CborErrorOutOfMemory) {
      break;
    }

    for (size_t i = 0; i < _ram_partition->header.numKeys; i++) {
      ConfigKey_t key = _ram_partition->keys[i];
      err = cbor_encode_text_stringz(&map, key.keyBuffer);
      if (err != CborNoError && err != CborErrorOutOfMemory) {
        break;
      }

      bool internalSuccess = true;
      switch (key.valueType) {
      case UINT32:
        internalSuccess = getConfig(key.keyBuffer, key.keyLen, tmpU);
        if (internalSuccess) {
          err = cbor_encode_uint(&map, tmpU);
        }
        break;
      case INT32:
        internalSuccess = getConfig(key.keyBuffer, key.keyLen, tmpI);
        if (internalSuccess) {
          err = cbor_encode_int(&map, tmpI);
        }
        break;
      case FLOAT:
        internalSuccess = getConfig(key.keyBuffer, key.keyLen, tmpF);
        if (internalSuccess) {
          err = cbor_encode_float(&map, tmpF);
        }
        break;
      case STR:
        tmpSSize = MAX_STR_LEN_BYTES;
        memset(tmpS, 0, MAX_STR_LEN_BYTES);
        internalSuccess = getConfig(key.keyBuffer, key.keyLen, tmpS, tmpSSize);
        if (internalSuccess) {
          err = cbor_encode_text_string(&map, tmpS, tmpSSize);
        }
        break;
      case BYTES:
        tmpBSize = MAX_STR_LEN_BYTES;
        memset(tmpB, 0, MAX_STR_LEN_BYTES);
        internalSuccess = getConfig(key.keyBuffer, key.keyLen, tmpB, tmpBSize);
        if (internalSuccess) {
          err = cbor_encode_byte_string(&map, tmpB, tmpBSize);
        }
        break;
      }

      if (!internalSuccess || (err != CborNoError && err != CborErrorOutOfMemory)) {
        break; // out of for loop
      }
    }

    if (err == CborNoError || err == CborErrorOutOfMemory) {
      err = cbor_encoder_close_container(&encoder, &map);
    }

    if (err == CborErrorOutOfMemory) {
      size_t needed = cbor_encoder_get_extra_bytes_needed(&encoder);
      printf("Encoding failed, buffer too small. Need %zu more bytes than the %zu originally "
             "given, or %zu total.\n",
             needed, original_len, original_len + needed);
    } else if (err == CborNoError) {
      buffer_len = cbor_encoder_get_buffer_size(&encoder, buffer);
      success = true;
    }
  } while (0);

  return success;
}

bool Configuration::prepareCborEncoder(const char * key, size_t key_len, CborEncoder &encoder, uint8_t &keyIdx, bool &keyExists) {
    configASSERT(key);
    bool rval = false;
    do {
        if(key_len > MAX_KEY_LEN_BYTES) {
            break;
        }
        if(_ram_partition->header.numKeys >= MAX_NUM_KV) {
            break;
        }
        keyExists = findKeyIndex(key, key_len, keyIdx);
        if(!keyExists) {
            keyIdx = _ram_partition->header.numKeys;
        }
        if(snprintf(_ram_partition->keys[keyIdx].keyBuffer,sizeof(_ram_partition->keys[keyIdx].keyBuffer),"%s",key) < 0){
            break;
        }
        cbor_encoder_init(&encoder, _ram_partition->values[keyIdx].valueBuffer, sizeof(_ram_partition->values[keyIdx].valueBuffer), 0);
        rval = true;
    } while(0);
    return rval;
}


/*!
* sets uint32 config
* \param key[in] - null terminated key
* \param value[in] - value
* \returns - true if success, false otherwise.
*/
bool Configuration::setConfig(const char * key, size_t key_len, uint32_t value) {
    configASSERT(key);
    bool rval = false;
    CborEncoder encoder;
    uint8_t keyIdx;
    bool keyExists;
    do{
        if(!prepareCborEncoder(key, key_len, encoder, keyIdx, keyExists)){
            break;
        }
        if(cbor_encode_uint(&encoder, value)!= CborNoError) {
            break;
        }
        _ram_partition->keys[keyIdx].valueType = UINT32;
        _ram_partition->keys[keyIdx].keyLen = key_len;
        if(!keyExists){
            _ram_partition->header.numKeys++;
        }
        _needs_commit = true;
        rval = true;
    } while(0);
    return rval;
}

/*!
* sets int32 config
* \param key[in] -  null terminated key
* \param value[in] - value
* \returns - true if success, false otherwise.
*/
bool Configuration::setConfig(const char * key, size_t key_len, int32_t value) {
    configASSERT(key);
    bool rval = false;
    CborEncoder encoder;
    uint8_t keyIdx;
    bool keyExists;
    do{
        if(!prepareCborEncoder(key, key_len, encoder, keyIdx, keyExists)){
            break;
        }
        if(cbor_encode_int(&encoder, value)!= CborNoError) {
            break;
        }
        _ram_partition->keys[keyIdx].valueType = INT32;
        _ram_partition->keys[keyIdx].keyLen = key_len;
        if(!keyExists){
            _ram_partition->header.numKeys++;
        }
        _needs_commit = true;
        rval = true;
    } while(0);
    return rval;
}

/*!
* sets float config
* \param key[in] -  null terminated key
* \param value[in] - value
* \returns - true if success, false otherwise.
*/
bool Configuration::setConfig(const char * key, size_t key_len, float value) {
    configASSERT(key);
    bool rval = false;
    CborEncoder encoder;
    uint8_t keyIdx;
    bool keyExists;
    do {
        if(!prepareCborEncoder(key, key_len, encoder, keyIdx, keyExists)){
            break;
        }
        if(cbor_encode_float(&encoder, value)!= CborNoError) {
            break;
        }
        _ram_partition->keys[keyIdx].valueType = FLOAT;
        _ram_partition->keys[keyIdx].keyLen = key_len;
        if(!keyExists){
            _ram_partition->header.numKeys++;
        }
        _needs_commit = true;
        rval = true;
    } while(0);
    return rval;
}

/*!
* sets a null-terminated str config
* \param key[in] -  null terminated key
* \param value[in] - value
* \returns - true if success, false otherwise.
*/
bool Configuration::setConfig(const char * key, size_t key_len, const char *value, size_t value_len) {
    configASSERT(key);
    bool rval = false;
    CborEncoder encoder;
    uint8_t keyIdx;
    bool keyExists;
    do {
        if(!prepareCborEncoder(key, key_len, encoder, keyIdx, keyExists)){
            break;
        }
        if(cbor_encode_text_string(&encoder, value, value_len)!= CborNoError) {
            break;
        }
        _ram_partition->keys[keyIdx].valueType = STR;
        _ram_partition->keys[keyIdx].keyLen = key_len;
        if(!keyExists){
            _ram_partition->header.numKeys++;
        }
        _needs_commit = true;
        rval = true;
    } while(0);
    return rval;
}

/*!
* sets bytes config
* \param key[in] -  null terminated key
* \param value[in] - value
* \param value_len[in] - bytes len
* \returns - true if success, false otherwise.
*/
bool Configuration::setConfig(const char * key, size_t key_len, const uint8_t *value, size_t value_len) {
    configASSERT(key);
    bool rval = false;
    CborEncoder encoder;
    uint8_t keyIdx;
    bool keyExists;
    do {
        if(!prepareCborEncoder(key, key_len, encoder, keyIdx, keyExists)){
            break;
        }
        if(cbor_encode_byte_string(&encoder, value, value_len)!= CborNoError) {
            break;
        }
        _ram_partition->keys[keyIdx].valueType = BYTES;
        _ram_partition->keys[keyIdx].keyLen = key_len;
        if(!keyExists){
            _ram_partition->header.numKeys++;
        }
        _needs_commit = true;
        rval = true;
    } while(0);
    return rval;
}


/*!
* Sets any cbor conifguration to a given key,
* \param key[in] -  null terminated key
* \param value[in] - value buffer
* \param value_len[in] - buffer len
* \returns - true if success, false otherwise.
*/
bool Configuration::setConfigCbor(const char * key, size_t key_len, uint8_t *value, size_t value_len) {
    configASSERT(key);
    configASSERT(value);
    CborValue it;
    CborParser parser;
    uint8_t keyIdx;
    bool rval = false;
    do {
        if(key_len > MAX_KEY_LEN_BYTES) {
            break;
        }
        if(value_len > MAX_CONFIG_BUFFER_SIZE_BYTES || value_len == 0) {
            break;
        }
        if(cbor_parser_init(value, value_len, 0, &parser, &it) != CborNoError){
            break;
        }
        if(!cbor_value_is_valid(&it)){
            break;
        }
        if(!findKeyIndex(key, key_len, keyIdx)){
            if(_ram_partition->header.numKeys >= MAX_NUM_KV) {
                break;
            }
            keyIdx = _ram_partition->header.numKeys;
            if(snprintf(_ram_partition->keys[keyIdx].keyBuffer,sizeof(_ram_partition->keys[keyIdx].keyBuffer),"%s",key) < 0){
                break;
            }
        }
        ConfigDataTypes_e type;
        if(!cborTypeToConfigType(&it,type)) {
            break;
        }
        _ram_partition->keys[keyIdx].valueType = type;
        _ram_partition->keys[keyIdx].keyLen = key_len;
        memcpy(_ram_partition->values[keyIdx].valueBuffer, value, value_len);
        if(keyIdx == _ram_partition->header.numKeys){
            _ram_partition->header.numKeys++;
        }
        _needs_commit = true;
        rval = true;
    } while(0);
    return rval;
}

/*!
* Sets any cbor conifguration to a given key,
* \param cbor[in] -  cbor type to convert from
* \param configType[out] - config type to convert to
* \returns - true if success, false otherwise.
*/
bool Configuration::cborTypeToConfigType(const CborValue *value, ConfigDataTypes_e &configType) {
    bool rval = true;
    do {
        if(cbor_value_is_integer(value)){
            if(cbor_value_is_unsigned_integer(value)){
                configType = UINT32;
                break;
            } else {
                configType = INT32;
                break;
            }
        } else if(cbor_value_is_byte_string(value)){
            configType = BYTES;
            break;
        } else if(cbor_value_is_text_string(value)){
            configType = STR;
            break;
        } else if (cbor_value_is_float(value)){
            configType = FLOAT;
            break;
        }
        rval = false;
    } while(0);
    return rval;
}


/*!
* Gets a list of the keys stored in the configuration.
* \param num_stored_key[out] - number of keys stored
* \returns - map of keys
*/
const ConfigKey_t* Configuration::getStoredKeys(uint8_t &num_stored_keys){
    num_stored_keys = _ram_partition->header.numKeys;
    return _ram_partition->keys;
}

/*!
* Remove a key/value from the store.
* \param key[in] - null terminated key
* \returns - true if successful, false otherwise.
*/
bool Configuration::removeKey(const char * key, size_t key_len) {
    configASSERT(key);
    bool rval = false;
    uint8_t keyIdx;
    do {
        if(!findKeyIndex(key, key_len,keyIdx)){
            break;
        }
        if(_ram_partition->header.numKeys - 1 > keyIdx) { // if there are keys after, we need to move them up.
            memmove(&_ram_partition->keys[keyIdx],&_ram_partition->keys[keyIdx+1], (_ram_partition->header.numKeys - 1 - keyIdx) * sizeof(ConfigKey_t)); // shift keys
            memmove(&_ram_partition->values[keyIdx],&_ram_partition->values[keyIdx+1], (_ram_partition->header.numKeys - 1 - keyIdx) * sizeof(ConfigValue_t)); // shift values
        }
        _ram_partition->header.numKeys--;
        _needs_commit = true;
        rval = true;
    } while(0);
    return rval;
}

bool Configuration::findKeyIndex(const char * key, size_t len, uint8_t &idx) {
    bool rval = false;
    for(int i = 0; i < _ram_partition->header.numKeys; i++){
        if(strncmp(key, _ram_partition->keys[i].keyBuffer,len) == 0){
            idx = i;
            rval = true;
            break;
        }
    }
    return rval;
}

const char* Configuration::dataTypeEnumToStr(ConfigDataTypes_e type) {
    switch(type){
        case UINT32:
            return "uint32";
        case INT32:
            return "int32";
        case FLOAT:
            return "float";
        case STR:
            return "str";
        case BYTES:
            return "bytes";
    }
    configASSERT(false); // NOT_REACHED
}

bool Configuration::saveConfig(bool restart) {
    bool rval = false;
    do {
        _ram_partition->header.crc32 = crc32_ieee(reinterpret_cast<const uint8_t *>(&_ram_partition->header.version), (sizeof(ConfigPartition_t)-sizeof(_ram_partition->header.crc32)));
        if(!_flash_partition.write(CONFIG_START_OFFSET_IN_BYTES, reinterpret_cast<uint8_t *>(_ram_partition), sizeof(ConfigPartition_t), CONFIG_LOAD_TIMEOUT_MS)){
            break;
        }
        if (restart) {
            resetSystem(RESET_REASON_CONFIG);
        }
        rval = true;
    } while(0);
    return rval;
}

 bool Configuration::getValueSize(const char * key, size_t key_len, size_t &size) {
    configASSERT(key);
    bool rval = false;
    CborValue it;
    CborParser parser;
    do {
        if(!prepareCborParser(key, key_len, it, parser)){
            break;
        }
        ConfigDataTypes_e configType;
        if(!cborTypeToConfigType(&it, configType)){
            break;
        }
        switch(configType){
            case UINT32:{
                size = sizeof(uint32_t);
                break;
            }
            case INT32:{
                size = sizeof(int32_t);
                break;
            }
            case FLOAT: {
                size = sizeof(float);
                break;
            }
            case STR:
            case BYTES:{
                if(cbor_value_get_string_length(&it,&size) != CborNoError){
                    return false;
                }
            }
        }
        rval = true;
    } while(0);
    return rval;
 }

bool Configuration::needsCommit(void) {
    return _needs_commit;
}

} // namespace cfg
