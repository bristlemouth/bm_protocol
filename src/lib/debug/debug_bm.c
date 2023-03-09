/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"

/* FreeRTOS+CLI includes. */
#include "FreeRTOS_CLI.h"

#include <string.h>
#include <inttypes.h>
#include "debug.h"
#include "bm_network.h"
#include "lwip/inet.h"

static BaseType_t neighborsCommand( char *writeBuffer,
                                  size_t writeBufferLen,
                                  const char *commandString);

static const CLI_Command_Definition_t cmdGpio = {
  // Command string
  "bm",
  // Help string
  "bm:\n"
  " * bm neighbors - show neighbors + liveliness\n"
  " * bm topo - print network topology\n"
  " * bm info <ip addr> - Request fw info from device\n"
  " * bm caps - get pub/sub capabilities of other nodes on network\n",
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
        } else if (strncmp("topo", parameter,parameterStringLength) == 0) {
            bm_network_request_neighbor_tables();
            vTaskDelay(10);
            bm_network_print_topology(NULL, NULL, 0);
        } else if (strncmp("info", parameter, parameterStringLength) == 0) {
            BaseType_t addrStrLen;
            const char *addrStr = FreeRTOS_CLIGetParameter(
            commandString,
            2, // Get the second parameter (address)
            &addrStrLen);

            if(addrStr == NULL) {
                printf("ERR Invalid parameters\n");
                break;
            }

            ip_addr_t addr;
            if(!inet6_aton(addrStr, &addr)) {
                printf("ERR Invalid address\n");
                break;
            }

            bm_network_request_fw_info(&addr);

        } else if (strncmp("caps", parameter,parameterStringLength) == 0) {
            bm_network_request_caps();
        } else {
            printf("ERR Invalid paramters\n");
            break;
        }
    } while(0);

    return pdFALSE;
}
