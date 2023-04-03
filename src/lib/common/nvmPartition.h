#pragma once
#include "abstract_storage_driver.h"

class NvmPartition {
    public:
        NvmPartition(AbstractStorageDriver& storage_driver, uint32_t partition_offset, uint32_t partition_length);
        bool read(uint32_t offset, uint8_t *buffer, size_t len, uint32_t timeoutMs);
        bool write(uint32_t offset, uint8_t *buffer, size_t len, uint32_t timeoutMs);
    private:
        AbstractStorageDriver& _storage_driver;
        uint32_t _partition_offset;
        uint32_t _partition_length;
};