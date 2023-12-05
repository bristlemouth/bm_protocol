//
// Created by Genton Mo on 1/14/21.
//

#pragma once
#include "memfault/core/platform/debug_log.h"
#include "memfault/core/platform/device_info.h"
#include "memfault/core/data_packetizer.h"

#define MEMFAULT_COREDUMP_CHUNK_SIZE 4096

// Change this for smaller coredumps!
#define MEMFAULT_PLATFORM_COREDUMP_CAPTURE_STACK_ONLY 0

/*
  Worst-case single-task stack size to make sure the whole stack is saved. Currently,
  the largest task stacks are configMINIMAL_STACK_SIZE * 3 * 4 bytes, which equals 1536 bytes.
*/
#define MAX_STACK_SIZE 8192

#ifdef __cplusplus
extern "C" {
#endif
int memfault_platform_boot(void);
int memfault_platform_start(void);
void memfault_enable_mpu();
bool memfault_threadsafe_packetizer_get_chunk_from_source(void* chunk_buff, size_t* chunk_len, uint32_t src_mask);
bool memfault_threadsafe_packetizer_is_data_available_from_source(uint32_t src_mask);
uint32_t memfault_get_pc(void);
uint32_t memfault_get_lr(void);
#ifdef __cplusplus
}
#endif
