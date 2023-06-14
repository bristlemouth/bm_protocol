#include <string.h>
#include "stm32_flash.h"
#include "bsp.h"

#define MIN(a, b)  (((a) < (b)) ? (a) : (b))
#define MAX(a, b)  (((a) > (b)) ? (a) : (b))

bool flashErase(uint32_t addr, size_t len) {
  FLASH_EraseInitTypeDef eraseCfg = {0};
  bool rval = false;
  bool next_bank = false;
  uint32_t startAddr = addr;

  eraseCfg.TypeErase = FLASH_TYPEERASE_PAGES;

  do {
    // Trying to erase before flash start
    if (addr < FLASH_BASE) {
      break;
    }

    // Trying to erase past flash boundary
    if (addr + len > (FLASH_BASE + FLASH_SIZE)) {
      break;
    }

    // Address not page aligned
    if (addr % FLASH_PAGE_SIZE) {
      break;
    }

    // Trying to erase fractions of a page
    if(len % FLASH_PAGE_SIZE) {
      break;
    }

    if(startAddr < (FLASH_BASE + FLASH_BANK_SIZE)) {
      const uint32_t endAddr = MIN(startAddr + len, (FLASH_BASE + FLASH_BANK_SIZE));
      const uint32_t page = (startAddr - FLASH_BASE)/FLASH_PAGE_SIZE;
      const uint32_t numPages = ((endAddr - startAddr) % FLASH_PAGE_SIZE) ? (((endAddr - startAddr)/FLASH_PAGE_SIZE) + 1) : ((endAddr - startAddr)/FLASH_PAGE_SIZE);

      eraseCfg.Banks = FLASH_BANK_1;
      eraseCfg.Page = page;
      eraseCfg.NbPages = numPages;

      uint32_t error = 0;
      HAL_FLASH_Unlock();
      {
        int res = HAL_FLASHEx_Erase(&eraseCfg, &error);
        if (res != HAL_OK) {
          // debug_printf("Erase Error %lu\n", error);
          break;
        }
      }
      HAL_FLASH_Lock();

      if (endAddr == (addr + len)) {
        rval = true;
        break;
      }

      startAddr = endAddr;
      next_bank = true;
    }

    const uint32_t endAddr = addr + len;
    const uint32_t page = (next_bank) ? 0 : ((startAddr - FLASH_BASE)/FLASH_PAGE_SIZE);
    const uint32_t numPages = ((endAddr - startAddr) % FLASH_PAGE_SIZE) ? (((endAddr - startAddr)/FLASH_PAGE_SIZE) + 1) : ((endAddr - startAddr)/FLASH_PAGE_SIZE);

    eraseCfg.Banks = FLASH_BANK_2;
    eraseCfg.Page = page;
    eraseCfg.NbPages = numPages;

    uint32_t error = 0;
    HAL_FLASH_Unlock();
    {
      int res = HAL_FLASHEx_Erase(&eraseCfg, &error);
      if (res != HAL_OK) {
        // debug_printf("Erase Error %lu\n", error);
        break;
      }
    }
    HAL_FLASH_Lock();

    rval = true;
  } while (0);

  if(!rval){
    HAL_FLASH_Lock();
  }

  return rval;
}

bool flashWrite(uint32_t addr, const uint8_t * data, uint32_t len) {
  bool rval = false;
  do {
    HAL_FLASH_Unlock();
    // 128-bit writes for STM32U5
    uint32_t cache[4] = {0};

    for (size_t offset = 0; offset < len; offset += sizeof(cache)) {
      memcpy(&cache, &data[offset], sizeof(cache));

      const uint32_t res = HAL_FLASH_Program(FLASH_TYPEPROGRAM_QUADWORD, addr + offset, (uint32_t)&cache);

      if (res != HAL_OK) {
        break;
      }
    }

    // Wait for last flash write to complete, otherwise if code
    // tries to verify a small write immediately after, it will
    // still read the old value!
    FLASH_WaitForLastOperation(FLASH_TIMEOUT_VALUE);

    HAL_FLASH_Lock();

    rval = true;
  } while(0);

  if(!rval){
    HAL_FLASH_Lock();
  }

  return rval;
}
