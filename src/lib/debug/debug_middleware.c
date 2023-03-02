/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"

/* FreeRTOS+CLI includes. */
#include "FreeRTOS_CLI.h"

#include "middleware.h"

#include <string.h>

static BaseType_t middlewareCommand( char *writeBuffer,
                                  size_t writeBufferLen,
                                  const char *commandString);

static const CLI_Command_Definition_t cmdGpio = {
  // Command string
  "mw",
  // Help string
  "mw:\n"
  " * mw\n",
  // Command function
  middlewareCommand,
  // Number of parameters (variable)
  -1
};

void debugMiddlewareInit(void) {
  FreeRTOS_CLIRegisterCommand( &cmdGpio );
}

#define MESSAGE_SIZE (32)
static BaseType_t middlewareCommand( char *writeBuffer,
                                  size_t writeBufferLen,
                                  const char *commandString) {

  // Remove unused argument warnings
  ( void ) commandString;
  ( void ) writeBuffer;
  ( void ) writeBufferLen;

  do {
    struct pbuf *pbuf = pbuf_alloc(PBUF_TRANSPORT, MESSAGE_SIZE, PBUF_RAM);
    configASSERT(pbuf);
    memset(pbuf->payload, 0, MESSAGE_SIZE);
    snprintf(pbuf->payload, MESSAGE_SIZE, "hello middleware!\n");
    if (middleware_net_tx(pbuf) != 0){
      printf("Error sending middleware message\n");
      pbuf_free(pbuf);
    } else {
      printf("Sent!\n");
    }

  } while(0);

  return pdFALSE;
}
