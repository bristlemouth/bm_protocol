/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"

/* FreeRTOS+CLI includes. */
#include "FreeRTOS_CLI.h"

#include <string.h>
#include <stdlib.h>
#include "debug.h"
#include "bsp.h"
#include "gpio.h"
#include "debug_gpio.h"
#include "debug_spi.h"
#include "protected_spi.h"

static BaseType_t debugSPICommand( char *writeBuffer,
                                  size_t writeBufferLen,
                                  const char *commandString);
static const DebugSPI_t *interfaces;
static uint32_t ulTotalInterfaces;

static const CLI_Command_Definition_t cmdSPI = {
  // Command string
  "spi",
  // Help string
  "spi:\n"
  " * spi list - show available interfaces\n"
  " * spi cfg <interface> <cpha> <cpol>\n"
  " * spi tx <interface> <cspin> <bytes 00 11 22 ...>\n",
  // Command function
  debugSPICommand,
  // Number of parameters (variable)
  -1
};

void debugSPIInit(const DebugSPI_t *interfaceList, uint32_t numberOfInterfaces) {
  configASSERT(interfaceList != NULL);
  configASSERT(numberOfInterfaces > 0);

  interfaces = interfaceList;
  ulTotalInterfaces = numberOfInterfaces;

  FreeRTOS_CLIRegisterCommand( &cmdSPI );
}
static const DebugSPI_t * findInterface(uint8_t interfaceNum) {
  const DebugSPI_t *interface = NULL;

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
    const DebugSPI_t *interface = &interfaces[index];
    printf("SPI%c\n", (char)('0' + interface->number));
  }
}

static void spiConfig(SPIInterface_t *interface, uint8_t cpOL, uint8_t cpHA) {

  if(cpOL) {
    interface->handle->Init.CLKPolarity = SPI_POLARITY_HIGH;
  } else {
    interface->handle->Init.CLKPolarity = SPI_POLARITY_LOW;
  }

  if(cpHA) {
    interface->handle->Init.CLKPhase = SPI_PHASE_2EDGE;
  } else {
    interface->handle->Init.CLKPhase = SPI_PHASE_1EDGE;
  }

  if (HAL_SPI_Init(interface->handle) != HAL_OK) {
    printf("ERR\n");
  } else {
    printf("OK\n");
  }
}

static void debugSPITxRx(SPIInterface_t *interface, const DebugGpio_t *spin, const char* bytes) {

  // Calculate number of bytes from string length
  // (adding one to account for the possibly missing space at the end)
  size_t numBytes = (strnlen(bytes, 128) + 1)/3;

  // Allocate buffers for tx/rx
  uint8_t *txBuff = pvPortMalloc(numBytes);
  configASSERT(txBuff != NULL);

  uint8_t *rxBuff = pvPortMalloc(numBytes);
  configASSERT(rxBuff != NULL);

  // Convert string to bytes
  for (uint16_t byte = 0; byte < numBytes; byte++) {
    txBuff[byte] = strtoul(&bytes[byte * 3], NULL, 16);
  }

  // Make sure CS is high before dropping it
  IOWrite(spin->handle, 1);

  // Delay in case CS wasn't high
  vTaskDelay(2);

  SPIResponse_t status = spiTxRx(interface, spin->handle, numBytes, txBuff, rxBuff, 100);

  if(status == SPI_OK) {
    // Print received bytes
    printf("OK ");
    for (uint16_t byte = 0; byte < numBytes; byte++) {
      printf("%02X ", rxBuff[byte]);
    }
    printf("\n");
  } else {
    printf("ERR\n");
  }

  // Release rx/tx buffers
  vPortFree(txBuff);
  vPortFree(rxBuff);
}

static BaseType_t debugSPICommand(char *writeBuffer,
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
      printf("ERR Invalid parameters\n");
      break;
    }

    if (strncmp("list", parameter,parameterStringLength) == 0) {
      listInterfaces();
    } else if (strncmp("cfg", parameter, parameterStringLength) == 0) {
      const char *interfaceNum = FreeRTOS_CLIGetParameter(
                                    commandString,
                                    2,
                                    &parameterStringLength);

      const char *phA = FreeRTOS_CLIGetParameter(
                                    commandString,
                                    3,
                                    &parameterStringLength);

      const char *poL = FreeRTOS_CLIGetParameter(
                                    commandString,
                                    4,
                                    &parameterStringLength);
      if(interfaceNum != NULL){
        const DebugSPI_t * debugInterface = findInterface((uint8_t)(interfaceNum[0] - '0'));
        if (debugInterface != NULL) {
          if(phA != NULL && poL != NULL) {
            spiConfig(debugInterface->interface, !(phA[0] == '0'), !(poL[0] == '0'));
          } else {
            printf("ERR Invalid parameters\n");
          }
        } else {
          printf("ERR Invalid interface\n");
        }
      } else {
        printf("ERR No interface parameter given\n");
      }

    } else if (strncmp("tx", parameter, parameterStringLength) == 0) {
      const char *interfaceNum = FreeRTOS_CLIGetParameter(
                                    commandString,
                                    2,
                                    &parameterStringLength);

      BaseType_t csNameLen;
      const char *sname = FreeRTOS_CLIGetParameter(
                                    commandString,
                                    3,
                                    &csNameLen);

      const char *bytes = FreeRTOS_CLIGetParameter(
                                    commandString,
                                    4,
                                    &parameterStringLength);
      if (interfaceNum != NULL){
        const DebugSPI_t * debugInterface = findInterface((uint8_t)(interfaceNum[0] - '0'));
        if (interfaceNum != NULL && debugInterface != NULL) {
          const DebugGpio_t *spin = findGpio(sname, csNameLen);
          if (spin == NULL) {
            printf("ERR Invalid CS pin\n");
          } else {
            debugSPITxRx(debugInterface->interface, spin, bytes);
          }
        } else {
          printf("ERR Invalid interface\n");
        }
      } else {
        printf("ERR No interface parameter given\n");
      }

    } else {
      printf("ERR Invalid paramters\n");
      break;
    }

  } while(0);
  return pdFALSE;
}
