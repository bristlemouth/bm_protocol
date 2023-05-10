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
  "ncp <log/debug> <message>\n",
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
    const char *target;
    BaseType_t target_str_len;
    target = FreeRTOS_CLIGetParameter(
                    commandString,
                    1,
                    &target_str_len);
    
    if(target == NULL) {
      printf("Invalid params\n");
      break;
    }
  
    const char *msg;
    BaseType_t msg_str_len;
    msg = FreeRTOS_CLIGetParameter(
                    commandString,
                    2,
                    &msg_str_len);

    if(msg == NULL) {
      printf("Invalid params\n");
      break;
    }

    if(strncmp("debug", target, target_str_len) == 0) {
      if (ncp_tx(BM_NCP_DEBUG, reinterpret_cast<const uint8_t *>(msg), msg_str_len)) {
        printf("Failed to send!\n");
      } else {
        printf("Sent!\n");
      }
    } else if (strncmp("log", target, target_str_len) == 0) {
      if (ncp_tx(BM_NCP_LOG, reinterpret_cast<const uint8_t *>(msg), msg_str_len)) {
        printf("Failed to send!\n");
      } else {
        printf("Sent!\n");
      }
    } else {
      printf("Invalid params\n");
    }


  } while(0);


  return pdFALSE;
}

void debug_ncp_init() {
  FreeRTOS_CLIRegisterCommand( &cmd_debug_ncp );
}
