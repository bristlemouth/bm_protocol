extern "C" {
#include "bm_dfu_generic.h"
}
#include "bootutil/bootutil_public.h"
#include "configuration.h"
#include "flash_map_backend/flash_map_backend.h"
#include "lpm.h"
#include "nvmPartition.h"
#include "reset_reason.h"
#include "stm32_flash.h"
#include "sysflash/sysflash.h"

static constexpr char dfu_confirm_config_key[] = "dfu_confirm";

using namespace cfg;
extern cfg::Configuration *systemConfigurationPartition;
extern NvmPartition *dfu_partition_global;

BmErr bm_dfu_client_set_confirmed(void) {
  if (boot_set_confirmed() == 0) {
    return BmOK;
  }
  return BmEINVAL;
}

BmErr bm_dfu_client_set_pending_and_reset(void) {
  boot_set_pending(0);
  resetSystem(RESET_REASON_MCUBOOT);
  return BmOK;
}

BmErr bm_dfu_client_fail_update_and_reset(void) {
  resetSystem(RESET_REASON_UPDATE_FAILED); // Revert to the previous image.
  return BmOK;
}

BmErr bm_dfu_client_flash_area_open(const void **flash_area) {
  if (flash_area_open(FLASH_AREA_IMAGE_SECONDARY(0), (const struct flash_area **)flash_area) ==
      0) {
    return BmOK;
  }
  return BmEINVAL;
}

BmErr bm_dfu_client_flash_area_close(const void *flash_area) {
  flash_area_close((const struct flash_area *)flash_area);
  return BmOK;
}

BmErr bm_dfu_client_flash_area_write(const void *flash_area, uint32_t off, const void *src,
                                     uint32_t len) {
  if (flash_area_write((const struct flash_area *)flash_area, off, src, len) == 0) {
    return BmOK;
  }
  return BmEINVAL;
}

BmErr bm_dfu_client_flash_area_erase(const void *flash_area, uint32_t off, uint32_t len) {
  if (flash_area_erase((const struct flash_area *)flash_area, off, len) == 0) {
    return BmOK;
  }
  return BmEINVAL;
}

uint32_t bm_dfu_client_flash_area_get_size(const void *flash_area) {
  return flash_area_get_size((const struct flash_area *)flash_area);
}

bool bm_dfu_client_confirm_is_enabled(void) {
  uint32_t val = 1;
  if (!systemConfigurationPartition) {
    return false;
  }
  systemConfigurationPartition->getConfig(dfu_confirm_config_key, strlen(dfu_confirm_config_key),
                                       val);
  return val == 1;
}

void bm_dfu_client_confirm_enable(bool en) {
  uint32_t val = en;
  if (systemConfigurationPartition) {
    systemConfigurationPartition->setConfig(dfu_confirm_config_key, strlen(dfu_confirm_config_key),
                                         val);
    systemConfigurationPartition->saveConfig(true);
  }
}

BmErr bm_dfu_host_get_chunk(uint32_t offset, uint8_t *buffer, size_t len, uint32_t timeout_ms) {
  if (!dfu_partition_global) {
    return BmEINVAL;
  }
  if (!dfu_partition_global->read(offset, buffer, len, timeout_ms)) {
    return BmEINVAL;
  }
  return BmOK;
}

void bm_dfu_core_lpm_peripheral_active(void) { lpmPeripheralActive(LPM_DFU_BRISTLEMOUTH); }

void bm_dfu_core_lpm_peripheral_inactive(void) { lpmPeripheralInactive(LPM_DFU_BRISTLEMOUTH); }
