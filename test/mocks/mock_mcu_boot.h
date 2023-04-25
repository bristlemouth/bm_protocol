#pragma once
#include "fff.h"
#include <stdint.h>
#include "bootutil/bootutil_public.h"
#include "bootutil/image.h"
#include "flash_map_backend/flash_map_backend.h"
#include "stm32_flash.h"
#include "sysflash/sysflash.h"

DECLARE_FAKE_VALUE_FUNC(int, flash_area_write, const struct flash_area*, uint32_t, const void*, uint32_t);
DECLARE_FAKE_VALUE_FUNC(int, flash_area_open, uint8_t, const struct flash_area **);
DECLARE_FAKE_VALUE_FUNC(int, flash_area_erase, const struct flash_area *, uint32_t, uint32_t);
DECLARE_FAKE_VOID_FUNC(flash_area_close,const struct flash_area*);
DECLARE_FAKE_VALUE_FUNC(int, boot_set_pending,int);
