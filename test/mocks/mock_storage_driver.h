#pragma once

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "abstract_storage_driver.h"

class MockStorageDriver: public AbstractStorageDriver {
    public:
        MOCK_METHOD(bool,read,(uint32_t addr, uint8_t *buffer, size_t len, uint32_t timeoutMs));
        MOCK_METHOD(bool, write,(uint32_t addr, uint8_t *buffer, size_t len, uint32_t timeoutMs));
        MOCK_METHOD(bool, erase,(uint32_t addr, size_t len, uint32_t timeoutMs));
        MOCK_METHOD(bool, crc16, (uint32_t addr, size_t len, uint16_t &crc, uint32_t timeoutMs));
        MOCK_METHOD(uint32_t, getAlignmentBytes,());
        MOCK_METHOD(uint32_t, getStorageSizeBytes,());
};