extern "C" {
#include "bm_configs_generic.h"
}
#include "configuration.h"

const GenericConfigKey *bcmp_config_get_stored_keys(uint8_t *num_stored_keys,
                                                    BmConfigPartition partition) {
  return (const GenericConfigKey *)get_stored_keys(partition, *num_stored_keys);
}

bool bcmp_remove_key(const char *key, size_t key_len, BmConfigPartition partition) {
  return remove_key(partition, key, key_len);
}

bool bcmp_config_needs_commit(BmConfigPartition partition) { return needs_commit(partition); }

bool bcmp_commit_config(BmConfigPartition partition) {
  save_config(partition, true); // Reboot!
  return true;
}

bool bcmp_set_config(const char *key, size_t key_len, uint8_t *value, size_t value_len,
                     BmConfigPartition partition) {
  return set_config_cbor(partition, (const char *)key, key_len, value, value_len);
}

bool bcmp_get_config(const char *key, size_t key_len, uint8_t *value, size_t *value_len,
                     BmConfigPartition partition) {

  return get_config_cbor(partition, key, key_len, value, *value_len);
}

bool bm_cbor_type_to_config_type(const CborValue *value, GenericConfigDataTypes *configType) {
  bool ret = false;
  GenericConfigDataTypes return_config_type;
  ret = cbor_type_to_config(value, return_config_type);
  if (ret) {
    *configType = return_config_type;
  }
  return ret;
}
