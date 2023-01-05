#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "sysflash/sysflash.h"
#include "flash_map_backend/flash_map_backend.h"
#include "mcuboot_config/mcuboot_logging.h"
#include "mcuboot_config/mcuboot_assert.h"
#include "stm32_flash.h"

// Big thanks to https://interrupt.memfault.com/blog/mcuboot-overview

// Allow disabling if needed
#ifndef NO_MCUBOOT_VERIFY_WE
// Verify write/erase operations
#define MCUBOOT_VERIFY_WE 1
#endif

// NOTE: FLASH_START, BOOTLOADER_SIZE, APP_SIZE, and SCRATCH_SIZE must be provided
//        as compile definitions. (See src/CMakelists.txt)
#define FLASH_SECTOR_SIZE 2048
#define FLASH_OFFSET FLASH_START
#define BOOTLOADER_START_ADDRESS 0x0
#define APPLICATION_PRIMARY_START_ADDRESS (BOOTLOADER_START_ADDRESS + BOOTLOADER_SIZE)
#define APPLICATION_SECONDARY_START_ADDRESS (APPLICATION_PRIMARY_START_ADDRESS + APP_SIZE)
#define SCRATCH_START_ADDRESS (APPLICATION_SECONDARY_START_ADDRESS + APP_SIZE)

static const struct flash_area bootloader = {
  .fa_id = FLASH_AREA_BOOTLOADER,
  .fa_device_id = FLASH_DEVICE_INTERNAL_FLASH,
  .fa_off = BOOTLOADER_START_ADDRESS,
  .fa_size = BOOTLOADER_SIZE,
};

static const struct flash_area primary_img0 = {
  .fa_id = FLASH_AREA_IMAGE_PRIMARY(0),
  .fa_device_id = FLASH_DEVICE_INTERNAL_FLASH,
  .fa_off = APPLICATION_PRIMARY_START_ADDRESS,
  .fa_size = APP_SIZE,
};

static const struct flash_area secondary_img0 = {
  .fa_id = FLASH_AREA_IMAGE_SECONDARY(0),
  .fa_device_id = FLASH_DEVICE_INTERNAL_FLASH,
  .fa_off = APPLICATION_SECONDARY_START_ADDRESS,
  .fa_size = APP_SIZE,
};

static const struct flash_area scratch = {
  .fa_id = FLASH_AREA_IMAGE_SCRATCH,
  .fa_device_id = FLASH_DEVICE_INTERNAL_FLASH,
  .fa_off = SCRATCH_START_ADDRESS,
  .fa_size = SCRATCH_SIZE,
};

static const struct flash_area *s_flash_areas[] = {
  &bootloader,
  &primary_img0,
  &secondary_img0,
  &scratch,
};

#define ARRAY_SIZE(arr) sizeof(arr)/sizeof(arr[0])
static const struct flash_area *prv_lookup_flash_area(uint8_t id) {
  for (size_t i = 0; i < ARRAY_SIZE(s_flash_areas); i++) {
    const struct flash_area *area = s_flash_areas[i];
    if (id == area->fa_id) {
      return area;
    }
  }
  return NULL;
}

/*< Opens the area for use. id is one of the `fa_id`s */
int flash_area_open(uint8_t id, const struct flash_area **area_outp) {
  MCUBOOT_LOG_DBG("Open ID=%d", (int)id);

  const struct flash_area *area = prv_lookup_flash_area(id);
  *area_outp = area;
  return area != NULL ? 0 : -1;
}

void    flash_area_close(const struct flash_area * area) {
  (void)area;
}

/*< Reads `len` bytes of flash memory at `off` to the buffer at `dst` */
int     flash_area_read(const struct flash_area * area, uint32_t off, void *dst,
                     uint32_t len) {
  if (area->fa_device_id != FLASH_DEVICE_INTERNAL_FLASH) {
    return -1;
  }

  const uint32_t end_offset = off + len;
  if (end_offset > area->fa_size) {
    MCUBOOT_LOG_ERR("RD Out of Bounds (0x%lx vs 0x%lx)", end_offset, area->fa_size);
    return -1;
  }

  // internal flash is memory mapped so just dereference the address
  void *addr = (void *)(area->fa_off + off);
  memcpy(dst, addr, len);

  return 0;
}

/*< Writes `len` bytes of flash memory at `off` from the buffer at `src` */
int     flash_area_write(const struct flash_area *area, uint32_t off,
                     const void *src, uint32_t len) {
  if (area->fa_device_id != FLASH_DEVICE_INTERNAL_FLASH) {
    return -1;
  }

  const uint32_t end_offset = off + len;
  if (end_offset > area->fa_size) {
    MCUBOOT_LOG_ERR("WR Out of Bounds (0x%lx vs 0x%lx)", end_offset, area->fa_size);
    return -1;
  }

  const uint32_t addr = area->fa_off + off;
  MCUBOOT_LOG_DBG("WR Addr: 0x%08lx Length: %lu", (uint32_t)addr, (uint32_t)len);

  if(!flashWrite(FLASH_OFFSET + addr, src, len)) {
    return -1;
  }

#if MCUBOOT_VERIFY_WE
  if (memcmp((void *)addr, src, len) != 0) {
    MCUBOOT_LOG_ERR("WR Program Failed");
    return -1;
  }
#endif

  return 0;
}

/*< Erases `len` bytes of flash memory at `off` */
int     flash_area_erase(const struct flash_area *area, uint32_t off, uint32_t len) {
  if (area->fa_device_id != FLASH_DEVICE_INTERNAL_FLASH) {
    return -1;
  }

  if ((len % FLASH_SECTOR_SIZE) != 0 || (off % FLASH_SECTOR_SIZE) != 0) {
    MCUBOOT_LOG_ERR("ER Not aligned on sector Offset: 0x%lx Length: 0x%lx",
                    (uint32_t)off, (uint32_t)len);
    return -1;
  }

  const uint32_t start_addr = area->fa_off + off;
  MCUBOOT_LOG_DBG("ER Addr: 0x%08lx Length: %lu", (uint32_t)start_addr, (uint32_t)len);

  // Erase required flash area(s)
  if(!flashErase(FLASH_OFFSET + start_addr, len)) {
    return -1;
  }

#if MCUBOOT_VERIFY_WE
  for (size_t i = 0; i < len; i++) {
    uint8_t *val = (void *)(start_addr + i);
    if (*val != flash_area_erased_val(area)) {
      MCUBOOT_LOG_ERR("ER failed at 0x%lx", (uint32_t)val);
      return -1;
    }
  }
#endif

  return 0;
}

/*< Returns this `flash_area`s alignment */
size_t flash_area_align(const struct flash_area *area) {
  (void)area;
  return 8;
}

/*< What is value is read from erased flash bytes. */
uint8_t flash_area_erased_val(const struct flash_area *area) {
  (void)area;
  return 0xFF;
}

/*< Given flash area ID, return info about sectors within the area. */
int     flash_area_get_sectors(int fa_id, uint32_t *count,
                     struct flash_sector *sectors) {
  const struct flash_area *area = prv_lookup_flash_area(fa_id);
  if (area->fa_device_id != FLASH_DEVICE_INTERNAL_FLASH) {
    return -1;
  }

  // All sectors for the STM32 are the same size
  const size_t sector_size = FLASH_SECTOR_SIZE;
  uint32_t total_count = 0;
  for (size_t off = 0; off < area->fa_size; off += sector_size) {
    // Note: Offset here is relative to flash area, not device
    sectors[total_count].fs_off = off;
    sectors[total_count].fs_size = sector_size;
    total_count++;
  }

  *count = total_count;
  return 0;
}

/*< Returns the `fa_id` for slot, where slot is 0 (primary) or 1 (secondary).
    `image_index` (0 or 1) is the index of the image. Image index is
    relevant only when multi-image support support is enabled */
int     flash_area_id_from_multi_image_slot(int image_index, int slot) {
  (void)image_index; // Remove unused argument warning when logging disabled

  switch (slot) {
    case 0:
      return FLASH_AREA_IMAGE_PRIMARY(image_index);
    case 1:
      return FLASH_AREA_IMAGE_SECONDARY(image_index);
  }

  MCUBOOT_LOG_ERR("Unexpected Request: image_index=%d, slot=%d", image_index, slot);
  return -1; /* flash_area_open will fail on that */
}

/*< Returns the slot (0 for primary or 1 for secondary), for the supplied
    `image_index` and `area_id`. `area_id` is unique and is represented by
    `fa_id` in the `flash_area` struct. */
int     flash_area_id_to_multi_image_slot(int image_index, int area_id) {
 (void)image_index; // Remove unused argument warning when logging disabled
 if (area_id == FLASH_AREA_IMAGE_PRIMARY(image_index)) {
    return 0;
  }
  if (area_id == FLASH_AREA_IMAGE_SECONDARY(image_index)) {
    return 1;
  }
return -1;
}

int flash_area_id_from_image_slot(int slot) {
  return flash_area_id_from_multi_image_slot(0, slot);
}

uint8_t flash_area_get_device_id(const struct flash_area *fa) {
  (void)fa;
  // We only have one device right now
  return 0;
}
