#pragma once

#include "abstract_spi.h"

namespace spiflash {

class W25 : public AbstractSPI {
public:
    W25(SPIInterface_t *interface, IOPinHandle_t *csPin);
    bool eraseChip(void);
    bool read(uint32_t addr, uint8_t *buffer, size_t len);
    bool write(uint32_t addr, uint8_t *buffer, size_t len);
    bool eraseSector(uint32_t addr);
    bool crc32Checksum(uint32_t addr, size_t len, uint32_t &crc32);
private:
    bool readyToWrite(uint32_t timeoutMs);
    bool readStatus(uint8_t &status);
    bool checkWEL(uint32_t timeoutMs, bool set, bool feedWDT=false);
};

} // namespace spiflash 