/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"

/* FreeRTOS+CLI includes. */
#include "FreeRTOS_CLI.h"

#include <string.h>
#include <inttypes.h>
#include "debug.h"
#include "bsp.h"
#include "gpio.h"
#include "debug_gpio.h"
#include "i2c.h"
#include "bsp.h"
// #include "resourceManager.h"

static BaseType_t debugGpioCommand( char *writeBuffer,
                                  size_t writeBufferLen,
                                  const char *commandString);
static const DebugGpio_t *pins;
static uint32_t ulTotalPins;

static const CLI_Command_Definition_t cmdGpio = {
  // Command string
  "gpio",
  // Help string
  "gpio:\n"
  " * gpio list - show available pins\n"
  " * gpio <set|clr|get> <pin name>\n",
  // Command function
  debugGpioCommand,
  // Number of parameters (variable)
  -1
};

void debugGpioInit(const DebugGpio_t *pinList, uint32_t numberOfPins) {
  configASSERT(pinList != NULL);
  configASSERT(numberOfPins > 0);

  pins = pinList;
  ulTotalPins = numberOfPins;

  FreeRTOS_CLIRegisterCommand( &cmdGpio );
}

const DebugGpio_t * findGpio(const char *name, uint32_t nameLen) {
  const DebugGpio_t *pin = NULL;

  for(uint32_t index = 0; index < ulTotalPins; index++) {
    if(strncmp(pins[index].name, name, nameLen) == 0) {
      pin = &pins[index];
      break;
    }
  }

  return pin;
}

static void listGpios() {
  for(uint32_t index = 0; index < ulTotalPins; index++) {
    const DebugGpio_t *pin = &pins[index];
    printf("[%s] %s\n", pin->mode ? "OUT" : "IN ", pin->name);
  }
}

static void readGpio(const DebugGpio_t * pin) {
  uint8_t value;

  // TODO - check rval
  IORead(pin->handle, &value);

  printf("OK %" PRIi16 "\n", value);
}

static void writeGpio(const DebugGpio_t * pin, uint8_t value) {
#ifdef UPPER_DECK
  if (pin->handle == &UD_LED_EN) {
    resourceManagerResource_t *power5VResource = getPower5VResource();
    if (value == 0) {
      resourceManagerReleaseResource(power5VResource);
    } else {
      resourceManagerRequestResource(power5VResource);
    }
  }
#endif

  IOWrite(pin->handle, value);
  printf("OK\n");
}

static BaseType_t debugGpioCommand(char *writeBuffer,
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

    if (strcmp("list", parameter) == 0) {
      listGpios();
    } else if (strncmp("get", parameter, parameterStringLength) == 0) {
      const char *name = FreeRTOS_CLIGetParameter(
                            commandString,
                            2, // Get the second parameter (name)
                            &parameterStringLength);

      const DebugGpio_t * pin = findGpio(name, parameterStringLength);
      if (pin != NULL) {
        readGpio(pin);
      } else {
        printf("ERR Invalid pin name.\n");
      }

    } else if (strncmp("set", parameter, parameterStringLength) == 0) {
      const char *name = FreeRTOS_CLIGetParameter(
                            commandString,
                            2, // Get the second parameter (name)
                            &parameterStringLength);

      const DebugGpio_t * pin = findGpio(name, parameterStringLength);

      if (pin == NULL) {
        printf("ERR Invalid pin name.\n");
      } else if (pin->mode == GPIO_IN) {
        printf("ERR Cannot set input pin.\n");
      } else {
        writeGpio(pin, 1);
      }
    } else if (strncmp("clr", parameter, parameterStringLength) == 0) {
      const char *name = FreeRTOS_CLIGetParameter(
                            commandString,
                            2, // Get the second parameter (name)
                            &parameterStringLength);

      const DebugGpio_t * pin = findGpio(name, parameterStringLength);

      if (pin == NULL) {
        printf("ERR Invalid pin name.\n");
      } else if (pin->mode == GPIO_IN) {
        printf("ERR Cannot set input pin.\n");
      } else {
        writeGpio(pin, 0);
      }
    } else {
      printf("ERR Invalid paramters\n");
      break;
    }

  } while(0);
  return pdFALSE;
}
