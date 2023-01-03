//
// Created by Genton Mo on 1/18/21.
//

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"

/* FreeRTOS+CLI includes. */
#include "FreeRTOS_CLI.h"

#include "debug_memfault.h"
#include <string.h>
#include "debug.h"
#include "memfault_platform_core.h"

#include "memfault/core/data_export.h"
#include "memfault/core/data_packetizer.h"
#include "memfault/core/reboot_tracking.h"
#include "memfault/metrics/metrics.h"
#include "memfault/panics/coredump.h"

static BaseType_t prvMemfaultCommand(char *pcWriteBuffer,
                                  size_t xWriteBufferLen,
                                  const char *pcCommandString);

static const CLI_Command_Definition_t cmdMemfault = {
  // Command string
  "memfault",
  // Help string
  "memfault:\n"
  " * dump - dump chunks\n"
  " * trig - trigger serialization\n"
  " * print - print metrics\n",
  // Command function
  prvMemfaultCommand,
  // Number of parameters
  1
};

void debugMemfaultInit() {
  FreeRTOS_CLIRegisterCommand( &cmdMemfault );
}

void test_coredump_storage(void) {
  printf("Testing coredump storage\n");
  printf("Test begin ");
  if(memfault_coredump_storage_debug_test_begin()) {
    printf("OK\n");
    printf("Test end ");
    if(memfault_coredump_storage_debug_test_finish()) {
      printf("OK\n");
    } else {
      printf("ERR\n");
    }
  } else {
    printf("ERR\n");
  }
}

static BaseType_t prvMemfaultCommand(char *pcWriteBuffer,
                                  size_t xWriteBufferLen,
                                  const char *pcCommandString) {

  const char *pcParameter;
  BaseType_t lParameterStringLength;

  ( void ) pcWriteBuffer;
  ( void ) xWriteBufferLen;

  pcParameter = FreeRTOS_CLIGetParameter(
    pcCommandString,
    1, // Get the first parameter (command)
    &lParameterStringLength);

  if(strncmp("trig", pcParameter, lParameterStringLength) == 0) {
    memfault_metrics_heartbeat_debug_trigger();
  } else if(strncmp("print", pcParameter, lParameterStringLength) == 0) {
    memfault_metrics_heartbeat_debug_print();
  } else if(strncmp("test", pcParameter, lParameterStringLength) == 0) {
    test_coredump_storage();
  } else {
    printf("Unsupported command.\n");
  }
  return pdFALSE;
}
