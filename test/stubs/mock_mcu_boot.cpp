#include "mock_mcu_boot.h"

DEFINE_FAKE_VALUE_FUNC(int, flash_area_write, const struct flash_area*, uint32_t, const void*, uint32_t);
DEFINE_FAKE_VALUE_FUNC(int, flash_area_open, uint8_t, const struct flash_area **);
DEFINE_FAKE_VALUE_FUNC(int, flash_area_erase, const struct flash_area *, uint32_t, uint32_t);
DEFINE_FAKE_VOID_FUNC(flash_area_close,const struct flash_area*);
DEFINE_FAKE_VALUE_FUNC(int, boot_set_pending,int);
DEFINE_FAKE_VALUE_FUNC(int, boot_set_confirmed);
