#include "cli.h"
#include "debug.h"
#include "debug_tca9546a.h"
#include "FreeRTOS.h"
#include "FreeRTOS_CLI.h"
#include <string.h>
#include <stdlib.h>
#include <stdlib.h>

static TCA9546A* _tca9546a_device;

static BaseType_t tca9546aCommand( char *writeBuffer,
                                    size_t writeBufferLen,
                                    const char *commandString);

static const CLI_Command_Definition_t cmdTCA9546A = {
  // Command string
  "tca",
  // Help string
  "tca:\n"
  " * tca init - initialize and confirm we can talk with the tca\n"
  " * tca get - get the current channel\n"
  " * tca reset - reset the device and set channel to 0\n"
  " * tca set <channel> - set the channel (0-4)\n",
  // Command function
  tca9546aCommand,
  // Number of parameters
  -1
};

void debugTCA9546AInit(TCA9546A *tca_device) {
  _tca9546a_device = tca_device;
  FreeRTOS_CLIRegisterCommand( &cmdTCA9546A );
}

static BaseType_t tca9546aCommand( char *writeBuffer,
                  size_t writeBufferLen,
                  const char *commandString) {
  BaseType_t parameterStringLength;

  ( void ) writeBuffer;
  ( void ) writeBufferLen;

  do {
    if(!_tca9546a_device){
      printf("debugTCA9546A not initialized\n");
      break;
    }
    const char *parameter = FreeRTOS_CLIGetParameter(
      commandString,
      1,
      &parameterStringLength);
    if(parameter == NULL){
      printf("ERR Invalid paramters\n");
      break;
    }
    if(strncmp("init", parameter, parameterStringLength) == 0){
      if(_tca9546a_device->init()){
        printf("TCA initialized\n");
        break;
      }
      else{
        printf("Failed to initialize TCA!\n");
        break;
      }
    } else if (strncmp("get", parameter, parameterStringLength) == 0) {
      Channel_t _channel;
      if(_tca9546a_device->getChannel(&_channel)){
        printf("TCA channel: %u\n", (uint8_t)_channel);
        break;
      }
      else{
        printf("Failed to get TCA channel\n");
        break;
      }
    } else if (strncmp("reset", parameter, parameterStringLength) == 0){
      _tca9546a_device->hwReset();
      printf("TCA reset!\n");
      break;
    } else if (strncmp("set", parameter, parameterStringLength) == 0) {
      const char *channel_string = FreeRTOS_CLIGetParameter(
        commandString,
        2,
        &parameterStringLength);
      uint8_t channel = strtoul(channel_string, NULL, 10);
      if(channel != CH_4 && channel != CH_3 && channel != CH_2 && channel != CH_1 && channel != CH_NONE){
        printf("ERR Invalid Channel!\n");
        break;
      }
      else{
        if(_tca9546a_device->setChannel((Channel_t)channel)){
          printf("Channel set!\n");
          break;
        }
        else{
          printf("Failed to set channel\n!");
          break;
        }
      }
    } else {
      printf("ERR Invalid paramters\n");
      break;
    }
  } while(0);
  return pdFALSE;
}