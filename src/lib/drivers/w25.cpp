#include <string.h>
#include "w25.h"
#include "app_util.h"
#include <cstdio>
#include "watchdog.h"
#include "crc.h"

namespace spiflash {
// STATUS REGISTER BITS
#define W25_SR1_BUSY                        (1 << 0)
#define W25_SR1_WEL                         (1 << 1)
#define W25_SR1_BP0                         (1 << 2)
#define W25_SR1_BP1                         (1 << 3)
#define W25_SR1_BP2                         (1 << 4)
#define W25_SR1_TB                          (1 << 5)
#define W25_SR1_SEC                         (1 << 6)
#define W25_SR1_SRP0                        (1 << 7)

#define W25_RW_HEADER_LEN                   (4) // 1 byte for command and 3 bytes for address
#define W25_MANUFACTURER_ID                 0xEF
#define W25_ERASE_TIMEOUT_MS                (50)
#define W25_WRITE_TIMEOUT_MS                (15)
#define W25_PAGE_SIZE                       (256) // W25 can write 256 bytes at a time
#define W25_PAGE_MASK                       (W25_PAGE_SIZE - 1)
#define W25_NUM_PAGES_IN_SECTOR             (16)
#define W25_SECTOR_SIZE                     (4096)
#define W25_SECTOR_MASK                     (W25_SECTOR_SIZE - 1)
#define W25_SECTOR_NUM_BITS                 (12)
#define W25_CHIP_ERASE_TIMEOUT_MS           (100 * 1000)
#define W25_SECTOR_ERASE_TIMEOUT_MS         (400)
#define W25_MAX_ADDRESS                     (0x7FFFFF)

#define W25X40CL_DEVICE_ID                  0x12
#define W25X40CL_MEMORY_TYPE                0x30
#define W25X40CL_CAPACITY_ID                0x13
#define W25X40CL_CAPACITY_KBYTES            (512)

#define W25Q80DV_DEVICE_ID                  0x13
#define W25Q80DV_MEMORY_TYPE                0x40
#define W25Q80DV_CAPACITY_ID                0x14
#define W25Q80DV_CAPACITY_KBYTES            (1024)

#define W25Q64JVXGIQ_CAPCITY_BYTES          (8000000) // 8MB
#define W25Q64JVXGIQ_ALIGNMENT_BYTES        (4096) // Sector Alignment

typedef enum {
    WRITE_STATUS                = 0x01,
    PAGE_PROGRAM                = 0x02,
    READ_DATA                   = 0x03,
    WRITE_DISABLE               = 0x04,
    READ_STATUS_1               = 0x05,
    WRITE_ENABLE                = 0x06,
    FAST_READ                   = 0x0B,
    SECTOR_ERASE                = 0x20,
    READ_STATUS_2               = 0x35,
    PROGRAM_SECURITY            = 0x42,
    ERASE_SECURITY              = 0x44,
    READ_SECURITY               = 0x48,
    READ_UNIQUE_ID              = 0x4B,
    VOLATILE_SR_WRITE_ENABLE    = 0x50,
    BLOCK_ERASE_32              = 0x52,
    READ_SFDP                   = 0x5A,
    ENABLE_RESET                = 0x66,
    ERASE_PROGRAM_SUSPEND       = 0x75,
    ERASE_PROGRAM_RESUME        = 0x7A,
    MANUFACTURER_DEVICE_ID      = 0x90,
    RESET                       = 0x99,
    JEDEC_ID                    = 0x9F,
    RELEASE_POWERDOWN_ID        = 0xAB,
    POWER_DOWN                  = 0xB9,
    CHIP_ERASE                  = 0xC7,
    BLOCK_ERASE_64              = 0xD8,
} W25_Reg_t;

W25::W25(SPIInterface_t *interface, IOPinHandle_t *csPin) : AbstractSPI(interface, csPin){
    configASSERT(_interface);
    _mutex = xSemaphoreCreateMutex();
    configASSERT(_mutex != NULL);
}
/*!
 * Erase flash chip
 * \return true if success, false if fail.
*/
bool W25::eraseChip(uint32_t timeoutMs) {
    bool retv = false;
    if(xSemaphoreTake(_mutex, pdMS_TO_TICKS(timeoutMs)) == pdTRUE) {
        uint8_t txBuff = 0;
        /* Make sure the previous write finished */
        do {
            if(!readyToWrite(W25_WRITE_TIMEOUT_MS)) {
                printf("Timeout waiting for write to complete\n");
                break;
            }
            /* Enable writes */
            txBuff = WRITE_ENABLE;
            if(writeBytes(&txBuff, sizeof(txBuff), 10,true) != SPI_OK) {
                printf("Error sending WREN command\n");
                break;
            }

            if(!checkWEL(W25_WRITE_TIMEOUT_MS, true)) {
                printf("Timeout waiting for write to complete\n");
                break;
            }

            /* Erase Chip */
            txBuff = CHIP_ERASE;
            if(writeBytes(&txBuff, sizeof(txBuff), 10, true) != SPI_OK) {
                printf("Error sending Erase Chip command\n");
                break;
            }

            /* Ensure WEL is reset */
            if(!checkWEL(W25_CHIP_ERASE_TIMEOUT_MS, false, true)) {
                printf("Timeout waiting for write to complete\n");
                break;
            }
            retv = true;
            printf("Successfully erased chip\n");
        } while (!retv);
        xSemaphoreGive(_mutex);
    } else {
        printf("Failed to acquir W25 mutex.\n");
    }
    return retv;
}

/*!
 * Read from flash
 * \param[in] addr - address of flash
 * \param[out] buffer - pointer to buffer to write into.
 * \param[in] len - length of data
 * \return true if success, false if fail.
*/
bool W25::read(uint32_t addr, uint8_t *buffer, size_t len, uint32_t timeoutMs) {
    bool rval = false;
    if(xSemaphoreTake(_mutex, pdMS_TO_TICKS(timeoutMs)) == pdTRUE) {
        rval = _read(addr, buffer, len);
        xSemaphoreGive(_mutex);
    } else {
        printf("Failed to acquire W25 mutex.\n");
    }
    return rval;
}

/*!
 * Write to flash
 * \param[in] addr - address of flash
 * \param[in] buffer - pointer to buffer of data
 * \param[in] len - length of data
 * \return true if success, false if fail.
*/
bool W25::write(uint32_t addr, uint8_t *buffer, size_t len, uint32_t timeoutMs) {
    bool rval = false;
    if(xSemaphoreTake(_mutex, pdMS_TO_TICKS(timeoutMs)) == pdTRUE) {
        rval = _write(addr, buffer, len);
        xSemaphoreGive(_mutex);
    } else {
        printf("Failed to acquire W25 mutex.\n");
    }
    return rval;
}

bool W25::readyToWrite(uint32_t timeoutMs) {
    uint32_t startTime = xTaskGetTickCount();
    bool rval = false;

    do {
        uint8_t status;
        if(!readStatus(status)) {
            break;
        }
        rval = !(status & W25_SR1_BUSY);
    } while((rval == false) && timeRemainingTicks(startTime, pdMS_TO_TICKS(timeoutMs)));
    return rval;
}

bool W25::readStatus(uint8_t &status) {
    uint8_t txBuff[3];
    uint8_t rxBuff[3];

    txBuff[0] = READ_STATUS_1;
    bool rval = (writeReadBytes(rxBuff, sizeof(txBuff),txBuff,10,true) == SPI_OK);
    status = rxBuff[1];
    return rval;
}

bool W25::checkWEL(uint32_t timeoutMs, bool set, bool feedWDT) {
    uint32_t startTime = xTaskGetTickCount();
    bool rval = false;

    do {
        if(feedWDT){
            watchdogFeed();
        }
        uint8_t status;
        if(!readStatus(status)) {
            break;
        }

        if (set) {
            rval = (status & W25_SR1_WEL);
        } else {
            rval = !(status & W25_SR1_WEL);
        }
  } while((rval == false) && timeRemainingTicks(startTime, pdMS_TO_TICKS(timeoutMs)));
  return rval;
}

/*!
 * Erase a sector in flash.
 * \param[in] addr - address of flash
 * \return true if success, false if fail.
*/
bool W25::eraseSector(uint32_t addr, uint32_t timeoutMs) {
    bool rval = false;
    if(xSemaphoreTake(_mutex, pdMS_TO_TICKS(timeoutMs)) == pdTRUE) {
        rval = _eraseSector(addr);
        xSemaphoreGive(_mutex);
    } else {
        printf("Failed to acquire W25 mutex.\n");
    }
    return rval;
}

/*!
 * Compute the crc32 checksum for segment of flash
 * \param[in] addr - address of flash
 * \param[in] len - length of flash to compute checksum for
 * \param[out] crc32 - resulting crc32 (valid if return is true)
 * \return true if success, false if fail.
*/
bool W25::crc32Checksum(uint32_t addr, size_t len, uint32_t &crc32, uint32_t timeoutMs) {
    configASSERT((addr + len) < W25_MAX_ADDRESS);
    crc32 = 0;
    bool retval = true;
    if(xSemaphoreTake(_mutex, pdMS_TO_TICKS(timeoutMs)) == pdTRUE) {
        bool first_read = true;
        uint32_t readlen = len;
        uint32_t offset = 0;
        uint32_t blocklen = (readlen < W25_SECTOR_SIZE) ? readlen : W25_SECTOR_SIZE;
        uint8_t* read_buf = (uint8_t *)pvPortMalloc(blocklen);
        configASSERT(read_buf);
        while(readlen){
            printf("Reading addr %lu, size %lu \n", (addr + offset), blocklen);
            if(!_read(addr + offset, read_buf, blocklen)){
                printf("Write failed\n");
                retval = false;
                break;
            }
            if(first_read){
                crc32 = crc32_ieee(read_buf, blocklen);
                first_read = false;
            } else {
                crc32 = crc32_ieee_update(crc32, read_buf, blocklen);
            }
            offset += blocklen;
            readlen -= blocklen;
            blocklen = (readlen < W25_SECTOR_SIZE) ? readlen : W25_SECTOR_SIZE;
        }
        vPortFree(read_buf);
        xSemaphoreGive(_mutex);
    } else {
        retval = false;
        printf("Failed to acquire W25 mutex.\n");
    }
    return retval;
}

bool W25::_read(uint32_t addr, uint8_t *buffer, size_t len) {
    configASSERT(buffer);
    configASSERT(((addr + len) < W25_MAX_ADDRESS));
    bool rval = false;
    const size_t buffLen = len + W25_RW_HEADER_LEN;

    uint8_t *txBuff = (uint8_t *)pvPortMalloc(buffLen);
    configASSERT(txBuff != NULL);

    uint8_t *rxBuff = (uint8_t *)pvPortMalloc(buffLen);
    configASSERT(rxBuff != NULL);

    /* Clear tx buff */
    memset(txBuff, 0, buffLen);

    txBuff[0] = READ_DATA;
    txBuff[1] = (addr >> 16) & 0xFF;
    txBuff[2] = (addr >> 8) & 0xFF;
    txBuff[3] = addr & 0xFF;

    /* Make sure there's no write in progress! */
    if(readyToWrite(W25_WRITE_TIMEOUT_MS)) {
        if(writeReadBytes(rxBuff, buffLen, txBuff, 100, true) == SPI_OK){
            memcpy(buffer, &rxBuff[W25_RW_HEADER_LEN], len);
            rval = true;
        }
    } else {
        printf("Timeout waiting for write to complete\n");
    }

    vPortFree(txBuff);
    vPortFree(rxBuff);

    return rval;
}

bool W25::_write(uint32_t addr, uint8_t *buffer, size_t len) {
    configASSERT(buffer);
    configASSERT(((addr + len) < W25_MAX_ADDRESS));
    bool rval = true;
    /* Allocate mem for a page request to Flash */
    uint8_t *pageReqBuff = (uint8_t *)pvPortMalloc(W25_PAGE_SIZE + W25_RW_HEADER_LEN);
    configASSERT(pageReqBuff != NULL);

    /* Allocate mem to read out sector at a time */
    uint8_t *sectorBuff = (uint8_t*)pvPortMalloc(W25_SECTOR_SIZE);
    configASSERT(sectorBuff != NULL);

    /* Flash operation variables */
    size_t totalBytesRemaining = len;

    /* Sector variables */
    size_t numSectorBytesToModify;
    uint32_t startSectorAddr;
    uint32_t currSectorAddr;
    bool sectorWriteSuccess;
    uint16_t numSectors = 0;
    uint32_t totalBytesWritten = 0;
    uint32_t currAddr = addr;

    /* Page variables */
    uint32_t wrAddr = 0;

    /* Figure out how many sectors need to be modified */
    if(len & W25_SECTOR_MASK) {
        numSectors = (len >> W25_SECTOR_NUM_BITS) + 1;
    } else {
        numSectors = (len >> W25_SECTOR_NUM_BITS);
    }

    /* Check if end address in the middle of a sector and it's a different sector than the beginning (write straddles a sector). */
    if((((addr) / W25_SECTOR_SIZE) != ((len + addr) / W25_SECTOR_SIZE )) && \
        (len + addr) % W25_SECTOR_SIZE) {
        numSectors++;
    }

    startSectorAddr = (addr & (~W25_SECTOR_MASK));
    wrAddr = startSectorAddr;

    for (int i=0; i < numSectors; i++) {
        /* Reset sectorBuffer */
        memset(sectorBuff,0xFF, W25_SECTOR_SIZE);

        /* Set current Address (based on current sector and start offset) */
        currSectorAddr = startSectorAddr + (W25_SECTOR_SIZE * i);
        configASSERT(currSectorAddr == wrAddr);

        /* Determine num bytes to modify within sector */
        /* If starting in the middle of a sector */
        if(wrAddr < currAddr) {
            if(totalBytesRemaining < (W25_SECTOR_SIZE - (currAddr - wrAddr))) {
                numSectorBytesToModify = totalBytesRemaining;
            } else {
                numSectorBytesToModify = W25_SECTOR_SIZE - (currAddr - wrAddr);
            }
        /* If sector aligned */
        } else {
            if(totalBytesRemaining < W25_SECTOR_SIZE) {
                numSectorBytesToModify = totalBytesRemaining;
            }else {
                numSectorBytesToModify = W25_SECTOR_SIZE;
            }
        }

        if(!_read(currSectorAddr, sectorBuff, W25_SECTOR_SIZE)) {
            printf("Unable to read sector.\n");
            rval = false;
            break;
        }

        /* Modify Sector */
        if(wrAddr < currAddr) {
            memcpy(&sectorBuff[currAddr-wrAddr], &buffer[totalBytesWritten], numSectorBytesToModify);
        } else {
            memcpy(sectorBuff, &buffer[totalBytesWritten], numSectorBytesToModify);
        }
        totalBytesWritten += numSectorBytesToModify;
        totalBytesRemaining -= numSectorBytesToModify;
        currAddr += numSectorBytesToModify;
        /* Erase Sector in Flash before re-writing */
        if (!_eraseSector(currSectorAddr)) {
            printf("Unable to erase sector at addr: %lu\n", currSectorAddr);
            rval = false;
            break;
        }

        /* Set the wrAddr for Page Program to the current start of the Sector */
        sectorWriteSuccess = true;

        /* Iterate through pages in sector to re-write to flash */
        for (int j=0; j < W25_NUM_PAGES_IN_SECTOR; j++) {
            /* Reset pageReq Buffer */
            memset(pageReqBuff, 0xFF, (W25_PAGE_SIZE + W25_RW_HEADER_LEN));

            /* Make sure the previous write finished */
            if(!readyToWrite(W25_WRITE_TIMEOUT_MS)) {
                printf("Timeout waiting for write to complete\n");
                sectorWriteSuccess = false;
                break;
            }

            /* Enable writes */
            uint8_t writeEnReq = WRITE_ENABLE;
            if(writeBytes(&writeEnReq, sizeof(writeEnReq), 10,true) != SPI_OK) {
                printf("Error sending WREN command\n");
                sectorWriteSuccess = false;
                break;
            }

            /* Ensure WEL is set */
            if(!checkWEL(W25_WRITE_TIMEOUT_MS, true)) {
                printf("Timeout waiting for write to complete\n");
                sectorWriteSuccess = false;
                break;
            }

            pageReqBuff[0] = PAGE_PROGRAM;
            pageReqBuff[1] = (wrAddr >> 16) & 0xFF;
            pageReqBuff[2] = (wrAddr >> 8) & 0xFF;
            pageReqBuff[3] = wrAddr & 0xFF;

            memcpy(&pageReqBuff[W25_RW_HEADER_LEN], &sectorBuff[wrAddr - currSectorAddr], W25_PAGE_SIZE);

            if(writeBytes(pageReqBuff, W25_RW_HEADER_LEN + W25_PAGE_SIZE, 10, true) != SPI_OK) {
                printf("Error writing bytes\n");
                sectorWriteSuccess = false;
                break;
            }

            /* Ensure WEL is reset */
            if(!checkWEL(W25_WRITE_TIMEOUT_MS, false)) {
                printf("Timeout waiting for write to complete\n");
                sectorWriteSuccess = false;
                break;
            }

            /* Update Flash Write address */
            wrAddr += W25_PAGE_SIZE;
        }

        if (!sectorWriteSuccess)
        {
            printf("Unable to write to page in sector at addr: %lu\n", wrAddr);
            rval = false;
            break;
        }
    };

    vPortFree(pageReqBuff);
    vPortFree(sectorBuff);

    return rval;

}

bool W25::_eraseSector(uint32_t addr) {
    /* Ensure that address/offset is Sector Aligned */
    configASSERT((addr & W25_SECTOR_MASK) == 0);
    configASSERT(addr < W25_MAX_ADDRESS);
    bool retv = false;
    uint8_t txBuff[4] = {0};

    /* Make sure the previous write finished */
    do {
        if(!readyToWrite(W25_WRITE_TIMEOUT_MS)) {
            printf("Timeout waiting for write to complete\n");
            break;
        }

        /* Enable writes */
        txBuff[0] = WRITE_ENABLE;
        if(writeBytes(txBuff, 1, 10,true) != SPI_OK) {
            printf("Error sending WREN command\n");
            break;
        }

        /* check if WEL is set */
        if(!checkWEL(W25_WRITE_TIMEOUT_MS, true)) {
            printf("Timeout waiting for write to complete\n");
            break;
        }

        /* Erase Specific Sector */
        txBuff[0] = SECTOR_ERASE;
        txBuff[1] = (addr >> 16) & 0xFF;
        txBuff[2] = (addr >> 8) & 0xFF;
        txBuff[3] = addr & 0xFF;
        if(writeBytes(txBuff, sizeof(txBuff), 10,true) != SPI_OK) {
            printf("Error sending Erase Chip command\n");
            break;
        }

        if(!checkWEL(W25_SECTOR_ERASE_TIMEOUT_MS, false, true)) {
            printf("Timeout waiting for write to complete\n");
            break;
        }

        retv = true;
        printf("Successfully erased sector\n");
    } while (!retv);

    return retv;
}

uint32_t W25::getAlignmentBytes(void) {
    return W25Q64JVXGIQ_ALIGNMENT_BYTES;
}

uint32_t W25::getStorageSizeBytes(void) {
    return W25Q64JVXGIQ_CAPCITY_BYTES;
}

/*!
 * Erase flash. Note that we can only erase in minimum sizes of 4096 byte sectors, and aligned on a sector.
 * \param[in] addr - address of flash, aligned to 4096 bytes
 * \param[in] len - length of flash to erase
 * \param[in] timeout - timeout to wait for operation to complete.
 * \return true if success, false if fail.
*/
bool W25::erase(uint32_t addr, size_t len, uint32_t timeoutMs) {
    uint32_t erase_start_addr = addr;
    if((addr % W25_SECTOR_SIZE) != 0) {
        erase_start_addr = addr - (addr % W25_SECTOR_SIZE);
    }
    configASSERT(erase_start_addr + len + (len % W25_SECTOR_SIZE) < W25_MAX_ADDRESS);
    bool rval = false;
    if(xSemaphoreTake(_mutex, pdMS_TO_TICKS(timeoutMs)) == pdTRUE) {
        uint32_t current_addr = erase_start_addr;
        while(current_addr < (erase_start_addr + len)){
            if(!_eraseSector(current_addr)){
                break;
            }
            current_addr += W25_SECTOR_SIZE;
        }
        if(current_addr >= (erase_start_addr + len)){
            rval = true;
        }
        xSemaphoreGive(_mutex);
    } else {
        printf("Failed to acquire W25 mutex.\n");
    }
    return rval;
}

bool W25::crc16(uint32_t addr, size_t len, uint16_t &crc, uint32_t timeoutMs) {
    configASSERT((addr + len) < W25_MAX_ADDRESS);
    crc = 0;
    bool retval = true;
    if(xSemaphoreTake(_mutex, pdMS_TO_TICKS(timeoutMs)) == pdTRUE) {
        uint32_t readlen = len;
        uint32_t offset = 0;
        uint32_t blocklen = (readlen < W25_SECTOR_SIZE) ? readlen : W25_SECTOR_SIZE;
        uint8_t* read_buf = (uint8_t *)pvPortMalloc(blocklen);
        configASSERT(read_buf);
        while(readlen){
            printf("Reading addr %lu, size %lu \n", (addr + offset), blocklen);
            if(!_read(addr + offset, read_buf, blocklen)){
                printf("Write failed\n");
                retval = false;
                break;
            }
            crc = crc16_ccitt(crc, read_buf, blocklen);
            offset += blocklen;
            readlen -= blocklen;
            blocklen = (readlen < W25_SECTOR_SIZE) ? readlen : W25_SECTOR_SIZE;
        }
        vPortFree(read_buf);
        xSemaphoreGive(_mutex);
    } else {
        retval = false;
        printf("Failed to acquire W25 mutex.\n");
    }
    return retval;
}

} // namespace spiflash
