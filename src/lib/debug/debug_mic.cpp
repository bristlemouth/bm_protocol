/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"

/* FreeRTOS+CLI includes. */
#include "FreeRTOS_CLI.h"

#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "debug.h"
#include "mic.h"
#include "bsp.h"
#include "debug_mic.h"

static BaseType_t debugMicCommand( char *writeBuffer,
                                  size_t writeBufferLen,
                                  const char *commandString);

static const CLI_Command_Definition_t cmdMic = {
  // Command string
  "mic",
  // Help string
  "mic:\n"
  " * db <duration seconds> - print dB level for n seconds\n",
  // Command function
  debugMicCommand,
  // Number of parameters (variable)
  -1
};

static SAI_HandleTypeDef *_sai;

void debugMicInit(SAI_HandleTypeDef *saiHandle) {
  _sai = saiHandle;

  FreeRTOS_CLIRegisterCommand( &cmdMic );
}

static bool processSamplesRMS(const uint32_t *samples, uint32_t numSamples, void *args) {
  (void)args;

  printf("%0.1f dB\n", micGetDB(samples, numSamples));

  return true;
}

static BaseType_t debugMicCommand(char *writeBuffer,
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

    if (strncmp("db", parameter, parameterStringLength) == 0) {
      if(!micInit(_sai, NULL)) {
        printf("ERR Microphone not detected. Aborting\n");
        break;
      }

      const char *duration = FreeRTOS_CLIGetParameter(
                            commandString,
                            2, // Get the second parameter (duration)
                            &parameterStringLength);

      if(parameterStringLength == 0) {
        printf("ERR duration required\n");
        break;
      }

      uint32_t durationSec = strtoul(duration, NULL, 10);

      micSample(durationSec, processSamplesRMS, NULL);
    } else {
      printf("ERR Invalid paramters\n");
      break;
    }

  } while(0);


  return pdFALSE;
}
