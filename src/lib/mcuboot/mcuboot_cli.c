#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>

#include "FreeRTOS.h"
#include "task.h"
#include "FreeRTOS_CLI.h"

#include "mcuboot_cli.h"
#include "reset_reason.h"

#include "bsp.h"
#include "bootutil/bootutil_public.h"
#include "sysflash/sysflash.h"
#include "flash_map_backend/flash_map_backend.h"

static BaseType_t mcubootCommand( char *writeBuffer,
                                      size_t writeBufferLen,
                                      const char *commandString);

static const CLI_Command_Definition_t cmdHWCfg = {
  // Command string
  "update",
  // Help string
  "update:\n"
  " * update confirm\n",
  // Command function
  mcubootCommand,
  // Number of parameters (variable)
  1
};

void mcubootCliInit() {
  FreeRTOS_CLIRegisterCommand( &cmdHWCfg );
}

// Erase last sector in image slot so the boot pending stuff works (this would
// normally be done by the updater tool)
void eraseLastSector() {
  const struct flash_area *fap;
  if (flash_area_open(FLASH_AREA_IMAGE_SECONDARY(0), &fap) == 0) {
    configASSERT(fap->fa_size >= FLASH_PAGE_SIZE);
    if(flash_area_erase(fap, fap->fa_size - FLASH_PAGE_SIZE, FLASH_PAGE_SIZE) != 0) {
      printf("Error erasing last sector\n");
    } else {
      printf("OK\n");
    }
  } else {
    printf("Error opening flash area.\n");
  }
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
          printf("ERR1\n");
        }
      } else if(strncmp("sec", filename, filenameLen) == 0) {
        int boot_rval = boot_set_pending(0);
        if(boot_rval) {
          printf("Error setting boot pending!\n");
        } else {
          resetSystem(RESET_REASON_MCUBOOT);
        }
      }else if(strncmp("clr", filename, filenameLen) == 0) {
        eraseLastSector();
      } else {
        printf("ERR2\n");
      }
    } else {
      printf("ERR3\n");
    }
  } while(0);
  return pdFALSE;
}
