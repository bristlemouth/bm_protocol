#include "debug_ms5803.h"
#include "FreeRTOS.h"
#include "FreeRTOS_CLI.h"
#include <string.h>
#include <stdlib.h>
#include "debug.h"
#include "cli.h"

static MS5803* _ms5803 = NULL;

static BaseType_t ms5803Command( char *writeBuffer,
                                  size_t writeBufferLen,
                                  const char *commandString);

static const CLI_Command_Definition_t cmdMs5803 = {
  // Command string
  "pressure",
  // Help string
  "pressure:\n"
  " * init - initialize the device\n"
  " * read - read the raw temp and pressure\n"
  " * reset - reset the device\n"
  " * check - check the devices prom\n"
  " * sig - get the device signature\n",
  // Command function
  ms5803Command,
  // Number of parameters (variable)
  -1
};


void debugMs5803Init(MS5803* ms5803) {
    _ms5803 = ms5803;
    FreeRTOS_CLIRegisterCommand( &cmdMs5803 );
}

static BaseType_t ms5803Command( char *writeBuffer,
                                  size_t writeBufferLen,
                                  const char *commandString) {
    BaseType_t parameterStringLength;

    ( void ) writeBuffer;
    ( void ) writeBufferLen;

    do {
        if(!_ms5803){
            printf("ms5803 debug not initialized!\n");
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
        if (strncmp("read", parameter, parameterStringLength) == 0) {
            float temp, pressure;
            if(!_ms5803->readPTRaw(pressure, temp)){
                printf("failed to read ms5803\n");
                break;
            }
            printf("Temp:%f, Pressure:%f\n",temp, pressure);
        } else if (strncmp("reset", parameter, parameterStringLength) == 0) {
            if(!_ms5803->reset()){
                printf("Failed to reset device!\n");
                break;
            }
            printf("Device reset\n");
        } else if (strncmp("init", parameter, parameterStringLength) == 0) {
            if(!_ms5803->init()){
                printf("Failed to initialize\n");
                break;
            }
            printf("Initialized!\n");
        } else if (strncmp("check", parameter, parameterStringLength) == 0) {
            if(!_ms5803->checkPROM()){
                printf("PROM check failed!\n");
                break;
            }
            printf("PROM check passed!\n");
        } else if (strncmp("sig", parameter, parameterStringLength) == 0) {
            printf("Signature: %" PRIu32 "\n",_ms5803->signature());
        } else {
            printf("ERR Invalid paramters\n");
        }

    } while(0);
    return pdFALSE;
}
