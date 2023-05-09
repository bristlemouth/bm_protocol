#include <string.h>
#include "FreeRTOS.h"
#include "task.h"

// FreeRTOS+CLI includes.
#include "FreeRTOS_CLI.h"

#include "ncp_uart.h"

#include "debug.h"

static BaseType_t cmd_debug_ncp_fn( char *writeBuffer,
                                  size_t writeBufferLen,
                                  const char *commandString);

static const CLI_Command_Definition_t cmd_debug_ncp = {
  // Command string
  "ncp",
  // Help string
  "ncp <message>\n",
  // Command function
  cmd_debug_ncp_fn,
  // Number of parameters
  -1
};


static BaseType_t cmd_debug_ncp_fn(char *writeBuffer,
                            size_t writeBufferLen,
                            const char *commandString) {
  (void) writeBuffer;
  (void) writeBufferLen;
  (void) commandString;

  do {
    const char *command;
    BaseType_t command_str_len;
    command = FreeRTOS_CLIGetParameter(
                    commandString,
                    1,
                    &command_str_len);

    if(strncmp("message", command, command_str_len) == 0) {
      // TODO make the next param the message type so we can choose what ncp message type we send
      BaseType_t parameterStringLength = 0;
      const char *parameter = FreeRTOS_CLIGetParameter(
                                  commandString,
                                  2,
                                  &parameterStringLength);
      uint16_t len = strnlen(parameter, NCP_BUFF_LEN);
      uint8_t *buff = static_cast<uint8_t *>(pvPortMalloc(len));
      configASSERT(buff != NULL);
      memcpy(buff, parameter, len);
      if (!ncpTx(BCMP_NCP_DEBUG, buff, len)) {
        printf("Failed to send!\n");
        vPortFree(buff);
      } else {
        printf("Sent!\n");
      }
    } else {
      printf("Invalid arguments\n");
    }
  } while(0);


  return pdFALSE;
}

void debug_ncp_init() {
  FreeRTOS_CLIRegisterCommand( &cmd_debug_ncp );
}
