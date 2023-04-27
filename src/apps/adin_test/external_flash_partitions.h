#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>
#include <stdlib.h>
#include "bm_dfu_message_structs.h"

typedef struct ext_flash_partition {
    /**
     * This area's offset, relative to the beginning of its flash
     * device's storage.
     */
    uint32_t fa_off;

    /**
     * This area's size, in bytes.
     */
    uint32_t fa_size;
} ext_flash_partition_t;

extern const ext_flash_partition_t hardware_configuration;
extern const ext_flash_partition_t system_configuration;
extern const ext_flash_partition_t user_configuration;
extern const ext_flash_partition_t cli_configuration;
extern const ext_flash_partition_t dfu_configuration;
#define DFU_HEADER_OFFSET_BYTES (0)
#define DFU_IMG_START_OFFSET_BYTES (sizeof(bm_dfu_img_info_t))
_Static_assert(DFU_IMG_START_OFFSET_BYTES > DFU_HEADER_OFFSET_BYTES, "Invalid DFU image offset");

#ifdef __cplusplus
}
#endif
