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

uint32_t NvmPartition::size(void) {
    return _partition.fa_size;
}

bool NvmPartition::erase(uint32_t offset, size_t len, uint32_t timeoutMs) {
    configASSERT(offset + len + (len % _storage_driver.getAlignmentBytes()) < _partition.fa_size);
    return _storage_driver.erase(_partition.fa_off + offset, len, timeoutMs);
}

uint32_t NvmPartition::alignment(void) {
    return _storage_driver.getAlignmentBytes();
}

bool NvmPartition::crc16(uint32_t offset, size_t len, uint16_t &crc, uint32_t timeoutMs) {
    configASSERT(offset + len + (len % _storage_driver.getAlignmentBytes()) < _partition.fa_size);
    return _storage_driver.crc16(_partition.fa_off + offset, len, crc, timeoutMs);
}
