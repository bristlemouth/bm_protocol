#include "debug_bridge_power_controller.h"
#include "FreeRTOS.h"
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>


static BaseType_t debugPowerControllerCommand( char *writeBuffer,
                                  size_t writeBufferLen,
                                  const char *commandString);

static const CLI_Command_Definition_t cmdBrPwrCtl = {
  // Command string
  "bpc",
  // Help string
  "enable <0/1> - enable or disable the controller\n"
   "subsample <0/1> - enable or disable subsampling\n",
    // Command function
  debugPowerControllerCommand,
  // Number of parameters (variable)
  -1
};


BridgePowerController * _bridge_power_controller;
void debugBridgePowerControllerInit(BridgePowerController *bridge_power_controller) {
    configASSERT(bridge_power_controller);
    _bridge_power_controller = bridge_power_controller;
    FreeRTOS_CLIRegisterCommand( &cmdBrPwrCtl );
}


static BaseType_t debugPowerControllerCommand(char *writeBuffer,
                                      size_t writeBufferLen,
                                      const char *commandString) {
  const char *parameter;
  BaseType_t parameterStringLength;

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

    if (strncmp("enable", parameter, parameterStringLength) == 0) {

        const char *enableStr = FreeRTOS_CLIGetParameter(
                                commandString,
                                2, // Get the second parameter (duration)
                                &parameterStringLength);

        if(enableStr == NULL) {
            printf("ERR Invalid paramters\n");
            break;
        }
        if(strncmp("0", enableStr, parameterStringLength) == 0) {
            printf("disable bridge bus power\n");
            _bridge_power_controller->powerControlEnable(false);
        } else if (strncmp("1", enableStr, parameterStringLength) == 0){
            printf("enable bridge bus power\n");
            _bridge_power_controller->powerControlEnable(true);
        } else {
            printf("ERR Invalid paramters\n");
        }

    } else if (strncmp("subsample", parameter, parameterStringLength) == 0) {
        const char *enableStr = FreeRTOS_CLIGetParameter(
                                commandString,
                                2, // Get the second parameter (duration)
                                &parameterStringLength);

        if(enableStr == NULL) {
            printf("ERR Invalid paramters\n");
            break;
        }
        if(strncmp("0", enableStr, parameterStringLength) == 0) {
            printf("disable bridge bus power\n");
            _bridge_power_controller->subSampleEnable(false);
        } else if (strncmp("1", enableStr, parameterStringLength) == 0){
            printf("enable bridge bus power\n");
            _bridge_power_controller->subSampleEnable(true);
        } else {
            printf("ERR Invalid paramters\n");
        }
    } else {
      printf("ERR Invalid paramters\n");
      break;
    }

  } while(0);

  return pdFALSE;
}
