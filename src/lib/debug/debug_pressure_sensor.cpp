#include "debug_pressure_sensor.h"
#include "FreeRTOS.h"
#include "FreeRTOS_CLI.h"
#include <string.h>
#include <stdlib.h>
#include "debug.h"
#include "cli.h"

static AbstractPressureSensor* _pressure_sensor = NULL;

static BaseType_t pressureSensorCommand( char *writeBuffer,
                                  size_t writeBufferLen,
                                  const char *commandString);

static const CLI_Command_Definition_t cmdPressureSensor = {
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
  pressureSensorCommand,
  // Number of parameters (variable)
  -1
};


void debugPressureSensorInit(AbstractPressureSensor* pressure_sensor) {
    _pressure_sensor = pressure_sensor;
    FreeRTOS_CLIRegisterCommand( &cmdPressureSensor );
}

static BaseType_t pressureSensorCommand( char *writeBuffer,
                                  size_t writeBufferLen,
                                  const char *commandString) {
    BaseType_t parameterStringLength;

    ( void ) writeBuffer;
    ( void ) writeBufferLen;

    do {
        if(!_pressure_sensor){
            printf("pressure sensor debug not initialized!\n");
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
            if(!_pressure_sensor->readPTRaw(pressure, temp)){
                printf("failed to read pressure sensor\n");
                break;
            }
            printf("Temp:%f, Pressure:%f\n",temp, pressure);
        } else if (strncmp("reset", parameter, parameterStringLength) == 0) {
            if(!_pressure_sensor->reset()){
                printf("Failed to reset device!\n");
                break;
            }
            printf("Device reset\n");
        } else if (strncmp("init", parameter, parameterStringLength) == 0) {
            if(!_pressure_sensor->init()){
                printf("Failed to initialize\n");
                break;
            }
            printf("Initialized!\n");
        } else if (strncmp("check", parameter, parameterStringLength) == 0) {
            if(!_pressure_sensor->checkPROM()){
                printf("PROM check failed!\n");
                break;
            }
            printf("PROM check passed!\n");
        } else if (strncmp("sig", parameter, parameterStringLength) == 0) {
            printf("Signature: %" PRIu32 "\n",_pressure_sensor->signature());
        } else {
            printf("ERR Invalid paramters\n");
        }

    } while(0);
    return pdFALSE;
}
