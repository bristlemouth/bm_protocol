#include "nvmPartition.h"
#include "FreeRTOS.h"
#include <stdio.h>

NvmPartition::NvmPartition(AbstractStorageDriver& storage_driver, const ext_flash_partition_t &partition): 
        _storage_driver(storage_driver), _partition(partition) {
    configASSERT((_partition.fa_off + _partition.fa_size < storage_driver.getStorageSizeBytes()));
    configASSERT(_partition.fa_off % storage_driver.getAlignmentBytes() == 0);
}

bool NvmPartition::read(uint32_t offset, uint8_t *buffer, size_t len, uint32_t timeoutMs) {
    configASSERT(offset + len < _partition.fa_size);
    return _storage_driver.read(_partition.fa_off + offset, buffer, len, timeoutMs);
}

bool NvmPartition::write(uint32_t offset, uint8_t *buffer, size_t len, uint32_t timeoutMs) {
    configASSERT(offset + len < _partition.fa_size);
    return _storage_driver.write(_partition.fa_off+offset, buffer, len, timeoutMs);
}
