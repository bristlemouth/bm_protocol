#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>

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

#ifdef __cplusplus
}
#endif
