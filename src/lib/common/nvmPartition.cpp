#include "nvmPartition.h"
#include "FreeRTOS.h"
#include <stdio.h>

NvmPartition::NvmPartition(AbstractStorageDriver& storage_driver, uint32_t partition_offset, uint32_t partition_length): 
        _storage_driver(storage_driver), _partition_offset(partition_offset), _partition_length(partition_length) {
    configASSERT((_partition_offset+_partition_length < storage_driver.getStorageSizeBytes()));
    configASSERT(_partition_offset % storage_driver.getAlignmentBytes() == 0);
}

bool NvmPartition::read(uint32_t offset, uint8_t *buffer, size_t len, uint32_t timeoutMs) {
    configASSERT(offset + len < _partition_length);
    return _storage_driver.read(_partition_offset + offset, buffer, len, timeoutMs);
}

bool NvmPartition::write(uint32_t offset, uint8_t *buffer, size_t len, uint32_t timeoutMs) {
    configASSERT(offset + len < _partition_length);
    return _storage_driver.write(_partition_offset+offset, buffer, len, timeoutMs);
}
