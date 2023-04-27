#pragma once

#include <stdint.h>
#include "nvmPartition.h"

#ifdef __cplusplus
extern "C" {
#endif

void debugDfuInit(NvmPartition *dfu_cli_partition);

#ifdef __cplusplus
}
#endif
