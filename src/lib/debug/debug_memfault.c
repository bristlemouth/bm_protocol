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
#include "mbedtls_base64/base64.h"
#include "serial.h"

static SerialHandle_t *_serialConsoleHandle;

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
  " * print - print metrics\n"
  " * coredump - print coredump over serial\n",
  // Command function
  prvMemfaultCommand,
  // Number of parameters
  1
};

void debugMemfaultInit(SerialHandle_t *serialConsoleHandle) {
  _serialConsoleHandle = serialConsoleHandle;
  FreeRTOS_CLIRegisterCommand( &cmdMemfault );
}

void test_coredump_storage(void) {
  printf("Testing coredump storage\n");
  printf("Test begin ");
  uint32_t startTime = xTaskGetTickCount();
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
  uint32_t endTime = xTaskGetTickCount();
  printf("Duration: %lums\n", endTime-startTime);
}

#define COREDUMP_CHUNK_SIZE 4096
static void print_coredump() {
  size_t coredumpSize = 0;

  if(memfault_coredump_has_valid_coredump(&coredumpSize)) {
    uint8_t *chunkData = (uint8_t *)pvPortMalloc(COREDUMP_CHUNK_SIZE);
    configASSERT(chunkData);

    // Only get coredump from packetizer
    memfault_packetizer_set_active_sources(kMfltDataSourceMask_Coredump);
    size_t buffLen = COREDUMP_CHUNK_SIZE;
    while(memfault_packetizer_get_chunk(chunkData, &buffLen)) {
      size_t olen = 0;

      size_t b64Bytes = 0;
      mbedtls_base64_encode(NULL, 0, &b64Bytes, chunkData, buffLen);

      // Buffer will be freed by serial tx task
      uint8_t *b64Data = (uint8_t *)pvPortMalloc(b64Bytes + 1);
      configASSERT(b64Data);

      mbedtls_base64_encode(b64Data, b64Bytes, &olen, chunkData, buffLen);
      b64Data[olen] = '\n';

      SerialMessage_t encodedMessage = {b64Data, olen + 1, _serialConsoleHandle};
      if(xQueueSend(serialGetTxQueue(), &encodedMessage, 100) != pdTRUE) {
        // Free buffer if unable to send
        vPortFree(encodedMessage.buff);
      }

      vTaskDelay(5);

      // Reset buffer len
      buffLen = COREDUMP_CHUNK_SIZE;
    }

    // Restore active sources
    memfault_packetizer_set_active_sources(kMfltDataSourceMask_All);

  } else {
    printf("No coredump available :(\n");
  }
}

#define COREDUMP_CHUNK_SIZE 4096
static void print_coredump() {
  size_t coredumpSize = 0;

  if(memfault_coredump_has_valid_coredump(&coredumpSize)) {
    uint8_t *chunkData = (uint8_t *)pvPortMalloc(COREDUMP_CHUNK_SIZE);
    configASSERT(chunkData);

    // Only get coredump from packetizer
    memfault_packetizer_set_active_sources(kMfltDataSourceMask_Coredump);
    size_t buffLen = COREDUMP_CHUNK_SIZE;
    while(memfault_packetizer_get_chunk(chunkData, &buffLen)) {
      size_t olen = 0;

      size_t b64Bytes = 0;
      mbedtls_base64_encode(NULL, 0, &b64Bytes, chunkData, buffLen);

      // Buffer will be freed by serial tx task
      uint8_t *b64Data = (uint8_t *)pvPortMalloc(b64Bytes + 1);
      configASSERT(b64Data);

      mbedtls_base64_encode(b64Data, b64Bytes, &olen, chunkData, buffLen);
      b64Data[olen] = '\n';

      SerialMessage_t encodedMessage = {b64Data, olen + 1, _serialConsoleHandle};
      if(xQueueSend(serialGetTxQueue(), &encodedMessage, 100) != pdTRUE) {
        // Free buffer if unable to send
        vPortFree(encodedMessage.buff);
      }

      vTaskDelay(5);

      // Reset buffer len
      buffLen = COREDUMP_CHUNK_SIZE;
    }

    // Restore active sources
    memfault_packetizer_set_active_sources(kMfltDataSourceMask_All);

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
  } else if(strncmp("coredump", pcParameter, lParameterStringLength) == 0) {
    print_coredump();
  } else {
    printf("Unsupported command.\n");
  }
  return pdFALSE;
}
