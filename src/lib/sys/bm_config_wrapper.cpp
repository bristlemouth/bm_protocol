extern "C" {
#include "bm_configs_generic.h"
}
#include "nvmPartition.h"
#include "reset_reason.h"

extern NvmPartition *userConfigurationPartition;
extern NvmPartition *systemConfigurationPartition;
extern NvmPartition *hardwareConfigurationPartition;

bool bm_config_read(BmConfigPartition partition, uint32_t offset, uint8_t *buffer,
                    size_t length, uint32_t timeout_ms) {
  bool ret = false;

  switch (partition) {
  case BM_CFG_PARTITION_USER:
    ret = userConfigurationPartition->read(offset, buffer, length, timeout_ms);
    break;
  case BM_CFG_PARTITION_SYSTEM:
    ret = systemConfigurationPartition->read(offset, buffer, length, timeout_ms);
    break;
  case BM_CFG_PARTITION_HARDWARE:
    ret = hardwareConfigurationPartition->read(offset, buffer, length, timeout_ms);
    break;
  default:
    break;
  }

  return ret;
}

bool bm_config_write(BmConfigPartition partition, uint32_t offset, uint8_t *buffer,
                     size_t length, uint32_t timeout_ms) {
  bool ret = false;

  switch (partition) {
  case BM_CFG_PARTITION_USER:
    ret = userConfigurationPartition->write(offset, buffer, length, timeout_ms);
    break;
  case BM_CFG_PARTITION_SYSTEM:
    ret = systemConfigurationPartition->write(offset, buffer, length, timeout_ms);
    break;
  case BM_CFG_PARTITION_HARDWARE:
    ret = hardwareConfigurationPartition->write(offset, buffer, length, timeout_ms);
    break;
  default:
    break;
  }

  return ret;
}

void bm_config_reset(void) { resetSystem(RESET_REASON_CONFIG); }
