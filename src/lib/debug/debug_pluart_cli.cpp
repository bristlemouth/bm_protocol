#include "debug_pluart_cli.h"
/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "payload_uart.h"

/* FreeRTOS+CLI includes. */
#include "FreeRTOS_CLI.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mbedtls_base64/base64.h"

static BaseType_t plUartCliCommand( char *writeBuffer,
                                 size_t writeBufferLen,
                                 const char *commandString);

//static bool plUartWrite(const unsigned char * buf, size_t binLen);

static const CLI_Command_Definition_t cmdPlUartCli = {
    // Command string
    "pluart",
    // Help string
    "pluart:\n"
    " * pluart write <ascii data>\n",
    // Command function
    plUartCliCommand,
    // Number of parameters (variable)
    -1
};

void debugPlUartCliInit(void) {
  FreeRTOS_CLIRegisterCommand( &cmdPlUartCli );
}

static BaseType_t plUartCliCommand( char *writeBuffer,
                                 size_t writeBufferLen,
                                 const char *commandString) {
  // Remove unused argument warnings
  ( void ) commandString;
  ( void ) writeBuffer;
  ( void ) writeBufferLen;

  do {
    BaseType_t opStrLen;
    const char *opStr = FreeRTOS_CLIGetParameter(
        commandString,
        1,
        &opStrLen);
    if(opStr == NULL) {
      printf("ERR Invalid parameters\n");
      break;
    }

    if (strncmp("write", opStr,opStrLen) == 0) {
      BaseType_t argDataLen;
      const char *argDataStr = FreeRTOS_CLIGetParameter(
          commandString,
          2,
          &argDataLen);
      if(argDataStr == NULL) {
        printf("ERR Invalid parameters\n");
        break;
      }
      // TODO - move this to an appropriate place in PLUART and guard with RS485 Half-Duplex flag
      /// For more context see: https://www.notion.so/bristlemouth/Internal-Bristlemouth-VEMCO-Receiver-Integration-043988c56f84454db1a31381fd825beb?pvs=4#85bbbf0ea1cf43a69e7624427f5e2356
      // Disable the Rx-enable (so we don't listen to ourselves).
      // Enable the Tx-enable (DE).
      IOWrite(&GPIO1, 1); // GPIO1 is connected to DE
      IOWrite(&GPIO2, 1); // GPIO2 is connected to RE
      // A 62 us setup delay is required, 1ms is the smallest thread-safe non-blocking RTOS delay we have?
      vTaskDelay(1);
      // TODO - make the termination configurable.
      const char* termination = {"\r\n"};
      PLUART::write((uint8_t*)argDataStr, strlen(argDataStr));
      PLUART::write((uint8_t*)termination, strlen(termination));
      // TODO - use postTxCb to disable Tx driver and enable receive when Tx is complete.
      /// Bodging in a 10ms hard-coded delay, which works for the RxLive, but will not work for all HD RS485 devices
      vTaskDelay(10);
      // Enable the Rx-receiver.
      // Disable the Tx-driver (DE). This is to save power, and may be required for some devices.
      IOWrite(&GPIO1, 0); // GPIO1 is connected to DE
      IOWrite(&GPIO2, 0); // GPIO2 is connected to RE
    } else {
      printf("ERROR - Unsupported op!: %s\n", opStr);
    }
  } while(0);

  return pdFALSE;
}
