#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>

#include "FreeRTOS.h"
#include "task.h"
#include "FreeRTOS_CLI.h"

#include "debug.h"
#include "device_info.h"
#include "log.h"
#include "mcuboot_cli.h"
#include "reset_reason.h"
#include "sd.h"
#include "serial_console.h"
#include "stm32_flash.h"
#include "util.h"
#include "watchdog.h"

#include "sysflash/sysflash.h"
#include "bootutil/bootutil_public.h"
#include "bootutil/image.h"
#include "flash_map_backend/flash_map_backend.h"

static BaseType_t mcubootCommand( char *writeBuffer,
                                      size_t writeBufferLen,
                                      const char *commandString);

static const CLI_Command_Definition_t cmdHWCfg = {
  // Command string
  "update",
  // Help string
  "update:\n"
  " * update <filename> - update firmware from specified file\n",
  // Command function
  mcubootCommand,
  // Number of parameters (variable)
  1
};

void mcubootCliInit() {
  FreeRTOS_CLIRegisterCommand( &cmdHWCfg );
}

#define VERSION_BLOCK_SIZE 4096

/*
  \brief Function to flash the second slot dedicated for the mcuboot from a file located on the SD card, then restart to use this new image

  \param[in] *filename - name of the binary file on the SD card to read and write to flash
  \param[in] *bufferSize - size of buffer to be used to read chunks from file
  \param[in] *permanent - used to set swap type between test(when false) and permanent(when true)
  \return value from enum to indicate a retry, no retry, or already up to date status, funciton will not return when an mcuboot reset occurs
*/
MCUBootUpdateResult_t mcubootFlashFromFile(const char *filename, const size_t bufferSize, bool permanent) {

  MCUBootUpdateResult_t rval = kMCUBootUpdateRetry;

  FIL *fil = pvPortMalloc(sizeof(FIL));
  configASSERT(fil != 0);
  memset(fil, 0, sizeof(FIL));

  // Allocate buffer to read data info before flashing
  uint8_t *buffer = pvPortMalloc(bufferSize);
  configASSERT(buffer);

  struct image_header *header = pvPortMalloc(sizeof(struct image_header));
  configASSERT(header);
  uint8_t *versionBlock = pvPortMalloc(VERSION_BLOCK_SIZE);
  configASSERT(versionBlock);
  const struct flash_area *fap;
  configASSERT(flash_area_open(FLASH_AREA_IMAGE_SECONDARY(0), &fap) == 0);

  do {
    unsigned int bytesRead;

    if(!sdIsMounted()) {
      printf("SD card not mounted.\n");
      rval = kMCUBootUpdateNoRetry;
      break;
    }

    FRESULT res = f_open (fil, filename, FA_READ);
    if ((res != FR_OK)) {
      printf("Could not open file %s(%d)\n", filename, res);
      // since we now check first that the file exists then we can retry if we have issues opening the file
      break;
    }

    res = f_read(fil, header, sizeof(struct image_header), &bytesRead);
    if ((res != FR_OK) || (bytesRead != sizeof(struct image_header))) {
      printf("Unable to read header\n");
      // break and retry
      break;
    }

    // Return file pointer to the start of the file
    f_lseek(fil, 0);

    if(header->ih_magic != IMAGE_MAGIC) {
      printf("Invalid image.\n");
      rval = kMCUBootUpdateNoRetry;
      break;
    }

    printf("Image size: %lu\n", header->ih_img_size);
    printf("Image version: %u.%u.%u+%08lX\n",
        header->ih_ver.iv_major,
        header->ih_ver.iv_minor,
        header->ih_ver.iv_revision,
        header->ih_ver.iv_build_num);

    printf("Fetching new images version info\n");

    res = f_read(fil, versionBlock, bufferSize, &bytesRead);
    if ((res != FR_OK) || (bytesRead != bufferSize)) {
      printf("Error reading from SD card\n");
      // break and retry
      break;
    }

    // Return file pointer to the start of the file
    f_lseek(fil, 0);

    const versionInfo_t *newImageVersion = findVersionInfo((uint32_t)versionBlock, bufferSize);
    const versionInfo_t *currImageVersion = getVersionInfo();

    if(!newImageVersion) {
      printf("Error: New verison info not found!\n");
      printf("Cancelling FW update\nMissing version info for new image\n");
      rval = kMCUBootUpdateNoRetry;
      break;
    }

    if(!currImageVersion) {
      printf("Error getting current version info\n");
      printf("Cancelling FW update\nMissing version info for current image\n");
      rval = kMCUBootUpdateNoRetry;
      break;
    }

    if(currImageVersion->hwVersion != newImageVersion->hwVersion) {
      printf("Cancelling FW update\nPlatform mismatch!\n");
      rval = kMCUBootUpdateNoRetry;
      break;
    }

    if(currImageVersion->gitSHA == newImageVersion->gitSHA){
      printf("Up to date!\n");
      rval = kMCUBootUpdateSuccess;
      break;
    }

    // If the current bootloader supports image signing the new image must be signed
    // so lets get the bootloader info and first check that it exists
    const versionInfo_t *bootloaderInfo = findVersionInfo(FLASH_START, BOOTLOADER_SIZE);
    if(!bootloaderInfo){
      printf("Caneclling FW update\nBootloader info not found!");
      rval = kMCUBootUpdateNoRetry;
      break;
    }

    if(bootloaderInfo->flags >> VER_SIGNATURE_SUPPORT_OFFSET & 0x1){
      if(!(newImageVersion->flags >> VER_SIGNATURE_SUPPORT_OFFSET & 0x1)){
        printf("Cancelling FW update\nBootloader requires signed images\nThis image is unsigned\n");
        rval = kMCUBootUpdateNoRetry;
        break;
      }
    }

    if(fwIsEng(newImageVersion)){
      logPrint(SYSLog, LOG_LEVEL_INFO, "Update attempt from %s to %s\n", getFWVersionStr(), newImageVersion->versionStr);
    }
    uint32_t size = f_size(fil);

    if(fap->fa_size < size) {
      printf("Firmware is too large!\n");
      rval = kMCUBootUpdateNoRetry;
      break;
    }

    // Round to the next block
    size += (0x800 - 1);
    size &= ~(0x800 - 1);

    // Erase old image
    printf("Erasing flash\n");
    if(flash_area_erase(fap, 0, size) != 0) {
      printf("Error erasing flash!\n");
      // break and retry
      break;
    }

    printf("Writing to flash\n");

    uint32_t address = 0;
    bool success = true;
    do {
      FRESULT read_res = f_read(fil, buffer, bufferSize, &bytesRead);

      // Done reading file!
      if ((read_res == FR_OK) && (bytesRead == 0)) {
        break;
      }

      if(read_res != FR_OK) {
        printf("ERR unable to read file (%d)\n", read_res);
        success = false;
        break;
      }

      serialConsolePutcharUnbuffered('.');
      if(flash_area_write(fap, address, buffer, bytesRead) != 0) {
        printf("ERR Flash write error. Aborting.\n");
        success = false;
        break;
      }
      address += bytesRead;
    } while(bytesRead == bufferSize);

    if(!success) {
      // break and retry
      break;
    }
    printf("\n");

    // Clean up, set pending update, and reboot!
    printf("OK\n");
    flash_area_close(fap);

    int boot_rval = boot_set_pending(permanent);
    if(boot_rval) {
      printf("Error setting boot pending! %d\n", rval);
      // break and retry
      break;
    }

    int swap_type = boot_swap_type();
    if((swap_type == BOOT_SWAP_TYPE_TEST) || (swap_type == BOOT_SWAP_TYPE_PERM)) {
      vTaskDelay(1000);

      resetSystem(RESET_REASON_MCUBOOT);
    } else {
      printf("Uh oh! boot_swap_type is %d, which is unexpected\n", swap_type);
      // no more need to break, but retry
    }
  } while(0);

  flash_area_close(fap);
  f_close(fil);
  vPortFree(versionBlock);
  vPortFree(header);
  vPortFree(buffer);
  vPortFree(fil);

  return rval;
}

static BaseType_t mcubootCommand(char *writeBuffer,
                                      size_t writeBufferLen,
                                      const char *commandString) {

  (void) writeBuffer;
  (void) writeBufferLen;

  do {
    const char *filename = NULL;
    BaseType_t filenameLen;
    filename = FreeRTOS_CLIGetParameter(
                    commandString, 1, &filenameLen);

    if(filename != NULL) {
      if(strncmp("confirm", filename, filenameLen) == 0) {
        if(boot_set_confirmed() == 0) {
          printf("OK\n");
        } else {
          printf("ERR\n");
        }
      } else {
        printf("Updating firmware with %s\n", filename);
        mcubootFlashFromFile(filename, 4096, false);
      }
    } else {
      printf("ERR\n");
    }
  } while(0);
  return pdFALSE;
}
