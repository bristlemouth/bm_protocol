/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"

/* FreeRTOS+CLI includes. */
#include "FreeRTOS_CLI.h"

#include <string.h>
#include <inttypes.h>
#include "debug.h"
#include "bm_network.h"

static BaseType_t neighborsCommand( char *writeBuffer,
                                  size_t writeBufferLen,
                                  const char *commandString);

static const CLI_Command_Definition_t cmdGpio = {
  // Command string
  "bm",
  // Help string
  "bm:\n"
  " * bm neighbors - show neighbors + liveliness\n",
  // Command function
  neighborsCommand,
  // Number of parameters (variable)
  -1
};

void debugBMInit(void) {
  FreeRTOS_CLIRegisterCommand( &cmdGpio );
}

static BaseType_t neighborsCommand( char *writeBuffer,
                                  size_t writeBufferLen,
                                  const char *commandString) {
    const char *parameter;
    BaseType_t parameterStringLength;

    // Remove unused argument warnings
    ( void ) commandString;
    ( void ) writeBuffer;
    ( void ) writeBufferLen;
  
    do {
        parameter = FreeRTOS_CLIGetParameter(
                    commandString,
                    1, // Get the first parameter (command)
                    &parameterStringLength);

        if(parameter == NULL) {
            printf("ERR Invalid paramters\n");
            break;
        }

        if (strncmp("neighbors", parameter,parameterStringLength) == 0) {
            bm_network_print_neighbor_table();
        } else {
            printf("ERR Invalid paramters\n");
            break;
        }
    } while(0);
    
    return pdFALSE;
}
