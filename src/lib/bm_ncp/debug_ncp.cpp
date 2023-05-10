#include <string.h>
#include "FreeRTOS.h"
#include "task.h"

// FreeRTOS+CLI includes.
#include "FreeRTOS_CLI.h"

#include "ncp.h"

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
  1
};


static BaseType_t cmd_debug_ncp_fn(char *writeBuffer,
                            size_t writeBufferLen,
                            const char *commandString) {
  (void) writeBuffer;
  (void) writeBufferLen;
  (void) commandString;

  do {
    const char *parameter;
    BaseType_t parameter_str_len;
    parameter = FreeRTOS_CLIGetParameter(
                    commandString,
                    1,
                    &parameter_str_len);

    if (ncp_tx(BCMP_NCP_DEBUG, reinterpret_cast<const uint8_t *>(parameter), parameter_str_len)) {
      printf("Failed to send!\n");
    } else {
      printf("Sent!\n");
    }

  } while(0);


  return pdFALSE;
}

void debug_ncp_init() {
  FreeRTOS_CLIRegisterCommand( &cmd_debug_ncp );
}
