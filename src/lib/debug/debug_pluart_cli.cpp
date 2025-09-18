#include "debug_pluart_cli.h"
/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "payload_uart.h"

/* FreeRTOS+CLI includes. */
#include "FreeRTOS_CLI.h"
#include "app_util.h"
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
      // TODO - make the termination configurable.
      const char* termination = {"\r\n"};
      // Use start and end transaction guards, have no effect if transactions not being used.
      PLUART::startTransaction();
      PLUART::write((uint8_t*)argDataStr, strlen(argDataStr));
      PLUART::write((uint8_t*)termination, strlen(termination));
      PLUART::endTransaction();

    } else {
      printf("ERROR - Unsupported op!: %s\n", opStr);
    }
  } while(0);

  return pdFALSE;
}
