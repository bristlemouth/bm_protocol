#pragma once

#include <inttypes.h>
#include <string.h>

class AbstractStorageDriver {
public:
    virtual bool read(uint32_t addr, uint8_t *buffer, size_t len, uint32_t timeoutMs) = 0;
    virtual bool write(uint32_t addr, uint8_t *buffer, size_t len, uint32_t timeoutMs) = 0;
    virtual bool erase(uint32_t addr, size_t len, uint32_t timeoutMs) = 0;
    virtual bool crc16(uint32_t addr, size_t len, uint16_t &crc, uint32_t timeoutMs) = 0;
    virtual uint32_t getAlignmentBytes(void) = 0;
    virtual uint32_t getStorageSizeBytes(void) = 0;
};
