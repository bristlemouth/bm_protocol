#include <string.h>
#include "FreeRTOS.h"
#include "task.h"

// FreeRTOS+CLI includes.
#include "FreeRTOS_CLI.h"

#include "bm_serial.h"

#include "debug.h"

static BaseType_t cmd_debug_ncp_fn( char *writeBuffer,
                                  size_t writeBufferLen,
                                  const char *commandString);

static const CLI_Command_Definition_t cmd_debug_ncp = {
  // Command string
  "ncp",
  // Help string
  "ncp <log/debug/pub/sub/unsub> <message>\n",
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

    if(command == NULL) {
      printf("Invalid params\n");
      break;
    }

    const char *arg1 = NULL;
    BaseType_t arg1_str_len = 0;
    arg1 = FreeRTOS_CLIGetParameter(
                    commandString,
                    2,
                    &arg1_str_len);

    const char *arg2 = NULL;
    BaseType_t arg2_str_len = 0;
    arg2 = FreeRTOS_CLIGetParameter(
                    commandString,
                    3,
                    &arg2_str_len);

    if(strncmp("debug", command, command_str_len) == 0) {
      if(arg1 == NULL) {
        printf("Invalid params\n");
        break;
      }

      if (bm_serial_tx(BM_NCP_DEBUG, reinterpret_cast<const uint8_t *>(arg1), arg1_str_len)) {
        printf("Failed to send!\n");
      } else {
        printf("Sent!\n");
      }
    } else if (strncmp("log", command, command_str_len) == 0) {
      if(arg1 == NULL) {
        printf("Invalid params\n");
        break;
      }

      if (bm_serial_tx(BM_NCP_LOG, reinterpret_cast<const uint8_t *>(arg1), arg1_str_len)) {
        printf("Failed to send!\n");
      } else {
        printf("Sent!\n");
      }
    } else if (strncmp("pub", command, command_str_len) == 0) {
      if(arg1 == NULL || arg2 == NULL) {
        printf("Invalid params\n");
        break;
      }

      // TODO - get and use node id
      if(bm_serial_pub(0, arg1, arg1_str_len, reinterpret_cast<const uint8_t *>(arg2), arg2_str_len) == 0) {
        printf("OK\n");
      } else {
        printf("ERR\n");
      }
    } else if (strncmp("sub", command, command_str_len) == 0) {
      if(arg1 == NULL) {
        printf("Invalid params\n");
        break;
      }
      if(bm_serial_sub(arg1, arg1_str_len) == 0) {
        printf("OK\n");
      } else {
        printf("ERR\n");
      }
    } else if (strncmp("unsub", command, command_str_len) == 0) {
      if(arg1 == NULL) {
        printf("Invalid params\n");
        break;
      }
      if(bm_serial_unsub(arg1, arg1_str_len) == 0) {
        printf("OK\n");
      } else {
        printf("ERR\n");
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
