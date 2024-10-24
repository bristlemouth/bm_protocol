extern "C" {
#include "bm_configs_generic.h"
}
#include "configuration.h"

using namespace cfg;

extern cfg::Configuration *userConfigurationPartition;
extern cfg::Configuration *sysConfigurationPartition;
extern cfg::Configuration *hwConfigurationPartition;

const GenericConfigKey *bcmp_config_get_stored_keys(uint8_t *num_stored_keys,
                                                    BmConfigPartition partition) {
  Configuration *cfg;
  if (partition == BM_CFG_PARTITION_USER) {
    cfg = userConfigurationPartition;
  } else if (partition == BM_CFG_PARTITION_SYSTEM) {
    cfg = sysConfigurationPartition;
  } else if (partition == BM_CFG_PARTITION_HARDWARE) {
    cfg = hwConfigurationPartition;
  } else {
    printf("Invalid configuration\n");
    return NULL;
  }
  return (const GenericConfigKey *)cfg->getStoredKeys(*num_stored_keys);
}

bool bcmp_remove_key(const char *key, size_t key_len, BmConfigPartition partition) {
  Configuration *cfg;
  if (partition == BM_CFG_PARTITION_USER) {
    cfg = userConfigurationPartition;
  } else if (partition == BM_CFG_PARTITION_SYSTEM) {
    cfg = sysConfigurationPartition;
  } else if (partition == BM_CFG_PARTITION_HARDWARE) {
    cfg = hwConfigurationPartition;
  } else {
    return false;
  }
  return cfg->removeKey(key, key_len);
}

bool bcmp_config_needs_commit(BmConfigPartition partition) {
  switch (partition) {
  case BM_CFG_PARTITION_USER: {
    return userConfigurationPartition->needsCommit();
  }
  case BM_CFG_PARTITION_SYSTEM: {
    return sysConfigurationPartition->needsCommit();
  }
  case BM_CFG_PARTITION_HARDWARE: {
    return hwConfigurationPartition->needsCommit();
  }
  default: {
    printf("Invalid partition\n");
    return false;
  }
  }
}

bool bcmp_commit_config(BmConfigPartition partition) {
  switch (partition) {
  case BM_CFG_PARTITION_USER: {
    userConfigurationPartition->saveConfig(); // Reboot!
    return true;
  }
  case BM_CFG_PARTITION_SYSTEM: {
    sysConfigurationPartition->saveConfig(); // Reboot!
    return true;
  }
  case BM_CFG_PARTITION_HARDWARE: {
    hwConfigurationPartition->saveConfig(); // Reboot!
    return true;
  }
  default:
    printf("Invalid partition\n");
    return false;
  }
}

bool bcmp_set_config(const char *key, size_t key_len, uint8_t *value, size_t value_len,
                     BmConfigPartition partition) {
  Configuration *cfg;
  if (partition == BM_CFG_PARTITION_USER) {
    cfg = userConfigurationPartition;
  } else if (partition == BM_CFG_PARTITION_SYSTEM) {
    cfg = sysConfigurationPartition;
  } else if (partition == BM_CFG_PARTITION_HARDWARE) {
    cfg = hwConfigurationPartition;
  } else {
    return false;
  }
  return cfg->setConfigCbor((const char *)key, key_len, value, value_len);
}

bool bcmp_get_config(const char *key, size_t key_len, uint8_t *value, size_t *value_len,
                     BmConfigPartition partition) {
  Configuration *cfg;
  if (partition == BM_CFG_PARTITION_USER) {
    cfg = userConfigurationPartition;
  } else if (partition == BM_CFG_PARTITION_SYSTEM) {
    cfg = sysConfigurationPartition;
  } else if (partition == BM_CFG_PARTITION_HARDWARE) {
    cfg = hwConfigurationPartition;
  } else {
    return false;
  }
  return cfg->getConfigCbor(key, key_len, value, *value_len);
}

bool bm_cbor_type_to_config_type(const CborValue *value, GenericConfigDataTypes *configType) {
  bool ret = false;
  ConfigDataTypes_e return_config_type;
  // TODO - have configuration.h use GenericConfigDataTypes
  ret = Configuration::cborTypeToConfigType(value, return_config_type);
  if (ret) {
    *configType = (GenericConfigDataTypes)return_config_type;
  }
  return ret;
}
