#pragma once
#include <stdint.h>
#include <stdlib.h>
#include "bm_serial_messages.h"
#include "nvmPartition.h"

bool ncp_dfu_start_cb(bm_serial_dfu_start_t *dfu_start);
bool ncp_dfu_chunk_cb(uint32_t offset, size_t length, uint8_t * data);
void ncp_dfu_init(NvmPartition *dfu_cli_partition);
void ncp_dfu_check_for_update(void);
