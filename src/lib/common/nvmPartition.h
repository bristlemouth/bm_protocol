#pragma once
#include "abstract_storage_driver.h"
#include "external_flash_partitions.h"

class NvmPartition {
    public:
        NvmPartition(AbstractStorageDriver& storage_driver, const ext_flash_partition_t& partition);
        bool read(uint32_t offset, uint8_t *buffer, size_t len, uint32_t timeoutMs);
        bool write(uint32_t offset, uint8_t *buffer, size_t len, uint32_t timeoutMs);
        bool erase(uint32_t offset, size_t len, uint32_t timeoutMs);
        uint32_t size(void);
        uint32_t alignment(void);
    private:
        AbstractStorageDriver& _storage_driver;
        const ext_flash_partition_t &_partition;
};