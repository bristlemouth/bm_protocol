#include "debug_htu.h"
#include "FreeRTOS.h"
#include "FreeRTOS_CLI.h"
#include <string.h>
#include <stdlib.h>
#include "debug.h"
#include "cli.h"

static HTU21D* _htu = NULL;

static BaseType_t htuCommand( char *writeBuffer,
                                  size_t writeBufferLen,
                                  const char *commandString);

static const CLI_Command_Definition_t cmdHtu = {
  // Command string
  "htu",
  // Help string
  "htu:\n"
  " r \n",
  // Command function
  htuCommand,
  // Number of parameters (variable)
  -1
};


void debugHtu21dInit(HTU21D* htu) {
    _htu = htu;
    FreeRTOS_CLIRegisterCommand( &cmdHtu );
}

static BaseType_t htuCommand( char *writeBuffer,
                                  size_t writeBufferLen,
                                  const char *commandString) {
    BaseType_t parameterStringLength;

    ( void ) writeBuffer;
    ( void ) writeBufferLen;

    do {
        if(!_htu){
            printf("HTU debug not initialized!\n");
            break;
        }
        const char *parameter = FreeRTOS_CLIGetParameter(
                commandString,
                1, // Get the first parameter (command)
                &parameterStringLength);

        if(parameter == NULL) {
            printf("ERR Invalid paramters\n");
            break;
        }
        if (strncmp("r", parameter, parameterStringLength) == 0) {
            float temp, humid;
            if(!_htu->read(temp, humid)){
                printf("failed to read htu\n");
                break;
            }
            printf("Temp:%f, Humid:%f\n",temp, humid);
        } else {
            printf("ERR Invalid paramters\n");
        }

    } while(0);
    return pdFALSE;
}
