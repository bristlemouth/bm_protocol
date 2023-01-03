/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"

/* FreeRTOS+CLI includes. */
#include "FreeRTOS_CLI.h"

#include <string.h>
#include <inttypes.h>
#include "bootloader_helper.h"
#include "bsp.h"
#include "debug.h"
#include "device_info.h"
#include "log.h"
#include "pca9535.h"
#ifdef HAS_REBOOTCTL
#include "rebootctl.h"
#endif
#include "reset_reason.h"
#include "sd.h"
#include "stm32_rtc.h"
#include "stm32l4xx_hal.h"
#include "sysMon.h"
#include "uptime.h"
#ifdef BOD_TEST
#undef HAS_NOTECARD
#endif
#ifdef HAS_NOTECARD
#include "notecard.h"
#ifndef NO_HWCFG
#include "hwcfg.h"
#endif
#endif

#ifdef USE_BOOTLOADER
#include "bootutil/bootutil_public.h"
#endif


#ifndef NO_IRIDIUM
#include "960x.h"
#endif

static BaseType_t infoCommand( char *writeBuffer,
                                  size_t writeBufferLen,
                                  const char *commandString);

static BaseType_t debugCommand(char *writeBuffer,
                                  size_t writeBufferLen,
                                  const char *commandString);

static BaseType_t uptimeCommand(char *writeBuffer,
                                  size_t writeBufferLen,
                                  const char *commandString);

static BaseType_t resetCommand( char *writeBuffer,
                                  size_t writeBufferLen,
                                  const char *commandString);

static BaseType_t bootloaderCommand( char *writeBuffer,
                                  size_t writeBufferLen,
                                  const char *commandString);

static const CLI_Command_Definition_t cmdInfo = {
  // Command string
  "info",
  // Help string
  "info:\n Display device information\n",
  // Command function
  infoCommand,
  // Number of parameters
  0
};

static const CLI_Command_Definition_t cmdUptime = {
  // Command string
  "uptime",
  // Help string
  "uptime:\n Show system uptime\n",
  // Command function
  uptimeCommand,
  // Number of parameters
  0
};

static const CLI_Command_Definition_t cmdDebug = {
  // Command string
  "debug",
  // Help string
  "debug:\n"
  " * reset - Reset device\n"
#if BUILD_DEBUG
  " * crash - Generate crash\n"
  " * hardfault - Generate hardfault\n"
  " * null - Dereference null pointer\n"
  " * mem - Get some memory statistics\n"
  " * tasks - Print task statistics\n"
  " * hang - Hang the process\n"
#endif
  " * bootloader - Restart and enter bootloader\n",
  // Command function
  debugCommand,
  // Number of parameters
  1
};

static const CLI_Command_Definition_t cmdReset = {
  // Command string
  "reset",
  // Help string
  "reset:\n Reset device\n",
  // Command function
  resetCommand,
  // Number of parameters
  0
};

static const CLI_Command_Definition_t cmdBootloader = {
  // Command string
  "bootloader",
  // Help string
  "bootloader:\n Restart and enter bootloader\n",
  // Command function
  bootloaderCommand,
  // Number of parameters
  0
};

void debugSysInit( ) {
  FreeRTOS_CLIRegisterCommand( &cmdInfo );
  FreeRTOS_CLIRegisterCommand( &cmdDebug );
  FreeRTOS_CLIRegisterCommand( &cmdUptime );
  FreeRTOS_CLIRegisterCommand( &cmdReset );
  FreeRTOS_CLIRegisterCommand( &cmdBootloader );
}

extern SD_HandleTypeDef hsd1;

// Check if debugger is attached
// For info see: https://developer.arm.com/documentation/ddi0403/d/Debug-Architecture/ARMv7-M-Debug/Debug-register-support-in-the-SCS/Debug-Halting-Control-and-Status-Register--DHCSR
static bool debuggerAttached() {
  return ((CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk) != 0);
}

static BaseType_t infoCommand( char *writeBuffer,
                                  size_t writeBufferLen,
                                  const char *commandString) {

  // Remove unused argument warnings
  ( void ) commandString;
  ( void ) writeBuffer;
  ( void ) writeBufferLen;

  const char *dirtyStr = "";
  if(fwIsDirty(getVersionInfo())){
    dirtyStr = "+";
  }

#ifdef APP_NAME
  printf("APP_NAME: %s\n", APP_NAME);
#endif
  printf("UID: %s\n", getUIDStr());

  printf("FW Version: %s\n", getFWVersionStr());

  printf("GIT SHA: %08lX%s\n", getGitSHA(), dirtyStr);

  printf("Build ID: ");
  const uint8_t *buildId;
  size_t buildIdLen = getBuildId(&buildId);
  for (uint32_t byte = 0; byte < buildIdLen; byte++) {
      printf("%02x", buildId[byte]);
  }
  printf("\n");

  printf("BSP: %s\n", BSP_NAME);

  printf("Reset Reason: %s\n", getResetReasonString());

#ifdef HAS_REBOOTCTL
  if(checkResetReason() == RESET_REASON_REBOOTCTL) {
    printf("RebootCtl Source: %lu\n", (uint32_t)rebootCtlGetLastSource());
  }
#endif

  printf("Main Deck HW Version: %02X\n", bspGetHwid());
  uint8_t udHwid = bspGetUdHwid();
  printf("Upper Deck HW Version: ");
  if (udHwid != 0xFF) {
    printf("%02X\n", udHwid);
  } else {
    printf("N/A\n");
  }

#ifndef NO_IRIDIUM
  char *imei = iridiumGetIMEI();
  printf("Iridium IMEI: ");
  if(imei != NULL) {
    printf("%s\n", imei);
  } else {
    printf("N/A\n");
  }
#endif

#ifdef USE_BOOTLOADER

  const versionInfo_t *bootloaderInfo = findVersionInfo(FLASH_START, BOOTLOADER_SIZE);

  if(bootloaderInfo) {
    const char *dirtyStr = "";
    if(fwIsDirty(bootloaderInfo)){
      dirtyStr = "+";
    }
    const char *engStr = "";
    if(fwIsEng(bootloaderInfo)) {
      engStr = "ENG-";
    }
    printf("Bootloader Information:\n");
    printf("  Version: %s%s\n", engStr, bootloaderInfo->versionStr);
    printf("  SHA: %08lX%s\n", bootloaderInfo->gitSHA, dirtyStr);
    printf("  Signature support: %lu\n", (bootloaderInfo->flags >> VER_SIGNATURE_SUPPORT_OFFSET) & 0x1);
    printf("  Encrytion support: %lu\n", (bootloaderInfo->flags >> VER_ENCRYPTION_SUPPORT_OFFSET) & 0x1);
  } else {
    printf("Unable to get bootloader information.\n");
  }
  printf("  Swap Type: %s\n", getSwapTypeStr());
#endif

#ifdef HAS_NOTECARD
#ifndef NO_HWCFG
  const hwCfg_t *config = hwCfgGet();
  if(config->hasCellular){
    char *noteimei = NULL;
    char *notecardSKU = NULL;
    char *notecardBoardVersion = NULL;
    char *notecardFirmwareVersion = NULL;
    notecardGetInfo(&noteimei, &notecardSKU, &notecardBoardVersion, &notecardFirmwareVersion, true);
    printf("Notecard Information:\n");
    printf("  Notecard IMEI: ");
    if(noteimei != NULL) {
      printf("%s\n", noteimei);
    } else {
      printf("N/A\n");
    }
    printf("  Notecard sku: ");
    if(notecardSKU != NULL) {
      printf("%s\n", notecardSKU);
    } else {
      printf("N/A\n");
    }
    printf("  Notecard Board Version: ");
    if(notecardBoardVersion != NULL) {
      printf("%s\n", notecardBoardVersion);
    } else {
      printf("N/A\n");
    }
    printf("  Notecard Firmware Version: ");
    if(notecardFirmwareVersion != NULL) {
      printf("%s\n", notecardFirmwareVersion);
    } else {
      printf("N/A\n");
    }
  }
#endif
#endif

  if(sdIsMounted()) {
    printf("SD Card Information:\n");
    if(hsd1.SdCard.CardType == CARD_SDSC) {
      printf("  Type: SDSC\n");
    } else if (hsd1.SdCard.CardType == CARD_SDHC_SDXC) {
      printf("  Type: SDHC/SDXC\n");
    } else {
      printf("  Unknown card type\n");
    }

    if(hsd1.SdCard.CardVersion == CARD_V2_X) {
      printf("  CardVersion: 2.x\n");
    } else {
      printf("  CardVersion: 1.x\n");
    }
    printf("  Class: %" PRIu32 "\n", hsd1.SdCard.Class);

    printf("  Block Size: %" PRIu32 "\n", hsd1.SdCard.BlockSize);
    printf("  Number of Blocks: %" PRIu32 "\n", hsd1.SdCard.BlockNbr);
    uint64_t capacity = sdGetCapacity();
    uint64_t currentCapacity = sdGetCurrentCapacity();
    uint64_t sdFreeBytes = sdGetFreeBytes();
    printf("  Size: %0.2fGB\n", capacity/1e9);
    if (sdFreeBytes/1e9 > 1){
      printf("  Current Partition: %0.2fGB (%0.02fGB free)\n", currentCapacity/1e9, (double)sdFreeBytes/1e9);
    }
    else{
      printf("  Current Partition: %0.2fMB (%0.02fMB free)\n", currentCapacity/1e6, (double)sdFreeBytes/1e6);
    }
  } else {
    printf("No SD card present.\n");
  }

  // Let user know debugger is attached
  if(debuggerAttached()) {
    printf("Debugger attached!\n");
  }

  printf("info end\n"); // terminator for production scripts

  return pdFALSE;
}

#define SECONDS_IN_HOUR (60 * 60)
#define SECONDS_IN_DAY (SECONDS_IN_HOUR * 24)
static BaseType_t uptimeCommand( char *writeBuffer,
                                  size_t writeBufferLen,
                                  const char *commandString) {

  // Remove unused argument warnings
  ( void ) commandString;
  ( void ) writeBuffer;
  ( void ) writeBufferLen;

  uint32_t uptime = uptimeGetSeconds();
  printf("Uptime:");
  if (uptime >= SECONDS_IN_DAY) {
    uint32_t days = uptime / SECONDS_IN_DAY;
    uptime -= (SECONDS_IN_DAY * days);
    printf(" %" PRIu32 " days", days);
  }

  float hours = (float)uptime / SECONDS_IN_HOUR;
  printf(" %0.2f hours\n", hours);

  return pdFALSE;
}

static HeapStats_t heapStats;

static BaseType_t getHeapStats() {

  vPortGetHeapStats(&heapStats);
  printf("********* HEAP STATS *********\n");
  printf("Available:            %zu\n", heapStats.xAvailableHeapSpaceInBytes);
  printf("Largest Free Block:   %zu\n", heapStats.xSizeOfLargestFreeBlockInBytes);
  printf("Smallest Free Block:  %zu\n", heapStats.xSizeOfSmallestFreeBlockInBytes);
  printf("Num free blocks:      %zu\n", heapStats.xNumberOfFreeBlocks);
  printf("Min ever free blocks: %zu\n", heapStats.xMinimumEverFreeBytesRemaining);
  printf("Total Allocs:         %zu\n", heapStats.xNumberOfSuccessfulAllocations);
  printf("Total Frees:          %zu\n", heapStats.xNumberOfSuccessfulFrees);
  printf("Alloc/Free delta:     %zu\n",
    heapStats.xNumberOfSuccessfulAllocations - heapStats.xNumberOfSuccessfulFrees);

  return pdFALSE;
}

#if configUSE_TRACE_FACILITY == 1
// Based on https://www.freertos.org/xTaskGetSystemState.html#TaskStatus_t

// Defined in sysMon.c
extern const char *taskStates[];

static void getSysStats() {
  TaskStatus_t *taskStatusArray;
  volatile UBaseType_t numberOfTasks;

  /* Take a snapshot of the number of tasks in case it changes while this
   function is executing. */
   numberOfTasks = uxTaskGetNumberOfTasks();

   /* Allocate a TaskStatus_t structure for each task.  An array could be
   allocated statically at compile time. */
   taskStatusArray = (TaskStatus_t*)pvPortMalloc( numberOfTasks * sizeof( TaskStatus_t ) );
   configASSERT(taskStatusArray != NULL);

   printf("%-15s | %-9s | pr | bPr | StackHighWaterMark\n","Task Name", "State");
   if( taskStatusArray != NULL ) {
    /* Generate raw status information about each task. */
    numberOfTasks = uxTaskGetSystemState( taskStatusArray,
                               numberOfTasks,
                               NULL );

    for(uint32_t taskIndex = 0; taskIndex < numberOfTasks; taskIndex++){
      printf("%-15s | %-9s | %3" PRIu32 " | %3" PRIu32 " | %" PRIu32 "\n",
          taskStatusArray[taskIndex].pcTaskName,
          taskStates[taskStatusArray[taskIndex].eCurrentState],
          taskStatusArray[taskIndex].uxCurrentPriority,
          taskStatusArray[taskIndex].uxBasePriority,
          taskStatusArray[taskIndex].usStackHighWaterMark
          );
    }

    /* The array is no longer needed, free the memory it consumes. */
    vPortFree( taskStatusArray );
  }
}
#endif

static BaseType_t debugCommand(char *writeBuffer,
                                  size_t writeBufferLen,
                                  const char *commandString) {

  const char *parameter;
  BaseType_t parameterStringLength;

  ( void ) writeBuffer;
  ( void ) writeBufferLen;

  parameter = FreeRTOS_CLIGetParameter(
                  commandString,
                  1, // Get the first parameter (command)
                  &parameterStringLength);

  if (strncmp("reset", parameter, parameterStringLength) == 0) {
    resetSystem(RESET_REASON_DEBUG_RESET);
#if BUILD_DEBUG
  } else if (strncmp("crash", parameter, parameterStringLength) == 0) {
    configASSERT(0);
  } else if (strncmp("hardfault", parameter, parameterStringLength) == 0) {
    volatile int32_t temp = 0;
    temp = 1/temp; // cppcheck-suppress zerodiv
  } else if (strncmp("null", parameter, parameterStringLength) == 0) {
    volatile uint32_t * temp = NULL;
    *temp = 0x1234; // cppcheck-suppress nullPointer
  } else if (strncmp("mem", parameter, parameterStringLength) == 0) {
    getHeapStats();
#if configUSE_TRACE_FACILITY == 1
  } else if (strncmp("tasks", parameter, parameterStringLength) == 0) {
    getSysStats();
#endif // configUSE_TRACE_FACILITY
  } else if (strncmp("hang", parameter, parameterStringLength) == 0) {
    while(1){};
#else // BUILD_DEBUG
    (void)getSysStats;
    (void)getHeapStats;
#endif // BUILD_DEBUG
  } else if (strncmp("bootloader", parameter, parameterStringLength) == 0) {
    rebootIntoROMBootloader();
  } else {
    printf("Unsupported command.\n");
  }

  return pdFALSE;
}

static BaseType_t resetCommand(char *writeBuffer,
                                size_t writeBufferLen,
                                const char *commandString) {
  ( void ) commandString;
  ( void ) writeBuffer;
  ( void ) writeBufferLen;

  resetSystem(RESET_REASON_DEBUG_RESET);

  return pdFALSE;
}

static BaseType_t bootloaderCommand(char *writeBuffer,
                                size_t writeBufferLen,
                                const char *commandString) {
  ( void ) commandString;
  ( void ) writeBuffer;
  ( void ) writeBufferLen;

  rebootIntoROMBootloader();

  return pdFALSE;
}
