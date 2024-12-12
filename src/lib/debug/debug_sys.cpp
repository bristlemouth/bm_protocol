/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"

/* FreeRTOS+CLI includes. */
#include "FreeRTOS_CLI.h"

#include <string.h>
#include <inttypes.h>
#include "bsp.h"
#include "debug.h"
#include "device_info.h"
#include "reset_reason.h"
#include "bootloader_helper.h"
#include "bsp.h"
#ifndef NO_NETWORK
#include "bristlemouth_client.h"
#endif

#ifdef USE_BOOTLOADER
#include "bootutil/bootutil_public.h"
#endif


static BaseType_t infoCommand( char *writeBuffer,
                                  size_t writeBufferLen,
                                  const char *commandString);

static BaseType_t debugCommand(char *writeBuffer,
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

static const CLI_Command_Definition_t cmdDebug = {
  // Command string
  "debug",
  // Help string
  "debug:\n"
  " * debug reset - Reset device\n"
  " * debug mem - Get some memory statistics\n"
  " * debug tasks - Print task statistics\n"
#if BUILD_DEBUG
  " * debug crash - Generate crash\n"
  " * debug hardfault - Generate hardfault\n"
  " * debug null - Dereference null pointer\n"
  " * debug hang - Hang the process\n"
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

#ifdef USE_BOOTLOADER
const char *getSwapTypeStr() {
  int32_t swap_type = boot_swap_type();
  switch (swap_type) {
      case BOOT_SWAP_TYPE_NONE: {
      return "NONE";
    }
    case BOOT_SWAP_TYPE_TEST: {
      return "TEST";
    }
    case BOOT_SWAP_TYPE_PERM: {
      return "PERM";
    }
    case BOOT_SWAP_TYPE_REVERT: {
      return "REVERT";
    }
    case BOOT_SWAP_TYPE_FAIL: {
      return "FAIL";
    }
    case BOOT_SWAP_TYPE_PANIC: {
      return "PANIC";
    }
    default: {
      return "UNKNOWN";
    }
  }
}
#else
const char *getSwapTypeStr() {
  return NULL;
}
#endif

void debugSysInit( ) {
  FreeRTOS_CLIRegisterCommand( &cmdInfo );
  FreeRTOS_CLIRegisterCommand( &cmdDebug );
  FreeRTOS_CLIRegisterCommand( &cmdReset );
  FreeRTOS_CLIRegisterCommand( &cmdBootloader );
}

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

  uint8_t mac[6];
  getMacAddr(mac, sizeof(mac));
  printf("MAC: ");
  for(uint8_t byte=0; byte < sizeof(mac)-1; byte++) {
    printf("%02x:", mac[byte]);
  }
  printf("%02x\n", mac[sizeof(mac)-1]);

#ifndef NO_NETWORK
  printf("IP addresses:\n");
  printf("Link Local: %s\n", bcl_get_ip_str(0));
  printf("Unicast:    %s\n", bcl_get_ip_str(1));
#endif

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

  printf("Node ID: %016llx\n", getNodeId());

#ifdef USE_BOOTLOADER

  const versionInfo_t *bootloaderInfo = findVersionInfo(FLASH_START, BOOTLOADER_SIZE);

  if(bootloaderInfo) {
    const char *bootDirtyStr = "";
    if(fwIsDirty(bootloaderInfo)){
      bootDirtyStr = "+";
    }
    const char *engStr = "";
    if(fwIsEng(bootloaderInfo)) {
      engStr = "ENG-";
    }
    printf("Bootloader Information:\n");
    printf("  Version: %s%s\n", engStr, bootloaderInfo->versionStr);
    printf("  SHA: %08lX%s\n", bootloaderInfo->gitSHA, bootDirtyStr);
    printf("  Signature support: %lu\n", (bootloaderInfo->flags >> VER_SIGNATURE_SUPPORT_OFFSET) & 0x1);
    printf("  Encrytion support: %lu\n", (bootloaderInfo->flags >> VER_ENCRYPTION_SUPPORT_OFFSET) & 0x1);
  } else {
    printf("Unable to get bootloader information.\n");
  }
  printf("  Swap Type: %s\n", getSwapTypeStr());
#endif

  // Let user know debugger is attached
  if(debuggerAttached()) {
    printf("Debugger attached!\n");
  }

  printf("info end\n"); // terminator for production scripts

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

const char *taskStates[] = {
    "Running",
    "Ready",
    "Blocked",
    "Suspended",
    "Deleted",
    "Invalid"
};

static void getSysStats() {
  TaskStatus_t *taskStatusArray;
  volatile UBaseType_t numberOfTasks;

  /* Take a snapshot of the number of tasks in case it changes while this
   function is executing. */
   numberOfTasks = uxTaskGetNumberOfTasks();

   /* Allocate a TaskStatus_t structure for each task.  An array could be
   allocated statically at compile time. */
   taskStatusArray = reinterpret_cast<TaskStatus_t*>(pvPortMalloc( numberOfTasks * sizeof( TaskStatus_t ) ));
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
  } else if (strncmp("hang", parameter, parameterStringLength) == 0) {
    while(1){};
#endif // BUILD_DEBUG
  } else if (strncmp("mem", parameter, parameterStringLength) == 0) {
    getHeapStats();
#if configUSE_TRACE_FACILITY == 1
  } else if (strncmp("tasks", parameter, parameterStringLength) == 0) {
    getSysStats();
#endif // configUSE_TRACE_FACILITY
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
