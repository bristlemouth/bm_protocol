#pragma once

#include <stdint.h>
#include "nvmPartition.h"

#ifdef __cplusplus
extern "C" {
#endif

void debugNvmCliInit(NvmPartition *debug_cli_nvm_partition, NvmPartition *dfu_cli_partition);

#ifdef __cplusplus
}
#endif
