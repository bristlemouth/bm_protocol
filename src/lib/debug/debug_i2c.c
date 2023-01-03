/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"

/* FreeRTOS+CLI includes. */
#include "FreeRTOS_CLI.h"

#include <string.h>
#include "debug.h"
#include "stm32l4xx_hal.h"
#include "i2c.h"
#include "debug_i2c.h"

static BaseType_t debugI2CCommand( char *writeBuffer,
                                      size_t writeBufferLen,
                                      const char *commandString);
static const DebugI2C_t *interfaces;
static uint32_t ulTotalInterfaces;

static const CLI_Command_Definition_t cmdI2C = {
  // Command string
  "i2c",
  // Help string
  "i2c:\n"
  " * i2c list\n"
  " * i2c scan <interface>\n",
  // Command function
  debugI2CCommand,
  // Number of parameters (variable)
  -1
};

void debugI2CInit(const DebugI2C_t *interfaceList, uint32_t numberOfInterfaces) {
  configASSERT(interfaceList != NULL);
  configASSERT(numberOfInterfaces > 0);

  interfaces = interfaceList;
  ulTotalInterfaces = numberOfInterfaces;

  FreeRTOS_CLIRegisterCommand( &cmdI2C );
}

static const DebugI2C_t * findInterface(uint8_t interfaceNum) {
  const DebugI2C_t *interface = NULL;

  for(uint32_t index = 0; index < ulTotalInterfaces; index++) {
    if(interfaces[index].number == interfaceNum) {
      interface = &interfaces[index];
      break;
    }
  }

  return interface;
}

static void listInterfaces() {
  for(uint32_t index = 0; index < ulTotalInterfaces; index++) {
    const DebugI2C_t *interface = &interfaces[index];
    printf("I2C%c\n", (char)('0' + interface->number));
  }
}

static void scanInterface(const DebugI2C_t *interface) {
  configASSERT(interface != NULL);
  printf("OK ");
  for(uint8_t address = 1; address < 128; address++) {
    I2CResponse_t status = i2cProbe(interface->handle, address, 5);
    if (I2C_OK == status) {
      printf("0x%02X ", address);
    }
  }
  printf("\n");
}

static BaseType_t debugI2CCommand(char *writeBuffer,
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

    if (strncmp("list", parameter,parameterStringLength) == 0) {
      listInterfaces();
    } else if (strncmp("scan", parameter, parameterStringLength) == 0) {
      const char *interfaceNum = FreeRTOS_CLIGetParameter(
                                    commandString,
                                    2, // Get the second parameter (name)
                                    &parameterStringLength);

      const DebugI2C_t * interface = findInterface((uint8_t)(interfaceNum[0] - '0'));
      if (interfaceNum != NULL && interface != NULL) {
        scanInterface(interface);
      } else {
        printf("ERR Invalid interface\n");
      }

    } else {
      printf("ERR Invalid paramters\n");
      break;
    }

  } while(0);
  return pdFALSE;
}
