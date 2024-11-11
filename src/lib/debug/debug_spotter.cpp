/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"

/* FreeRTOS+CLI includes. */
#include "FreeRTOS_CLI.h"

#include "bsp.h"
#include "debug.h"
#include "lwip/inet.h"
#include "pubsub.h"
#include "spotter.h"
#include "util.h"
#include <inttypes.h>
#include <string.h>

#ifdef BSP_BRIDGE_V1_0
#define LED_BLUE LED_G
#define ALARM_OUT TP10
#elif BSP_MOTE_V1_0
#define LED_BLUE GPIO1
#define ALARM_OUT GPIO2
#endif

static BaseType_t cmd_spotter_fn(char *writeBuffer, size_t writeBufferLen,
                                 const char *commandString);

static const CLI_Command_Definition_t cmdSpotter = {
    // Command string
    "spotter",
    // Help string
    "spotter:\n"
    " * spotter printf <string>\n"
    " * spotter fprintf <file_name> <string>\n"
    " * spotter txdata <i/c> <data>\n",
    // Command function
    cmd_spotter_fn,
    // Number of parameters (variable)
    -1};

void debugSpotterInit(void) { FreeRTOS_CLIRegisterCommand(&cmdSpotter); }

static BaseType_t cmd_spotter_fn(char *writeBuffer, size_t writeBufferLen,
                                 const char *commandString) {
  // Remove unused argument warnings
  (void)commandString;
  (void)writeBuffer;
  (void)writeBufferLen;

  do {
    BaseType_t command_str_len;
    const char *command = FreeRTOS_CLIGetParameter(commandString,
                                                   1, // Get the first parameter (command)
                                                   &command_str_len);

    if (command == NULL) {
      printf("ERR Invalid command\n");
      break;
    } else if (strncmp("printf", command, command_str_len) == 0) {
      const char *dataStr = FreeRTOS_CLIGetParameter(commandString, 2, &command_str_len);

      if (command_str_len == 0) {
        printf("ERR data required\n");
        break;
      }
      size_t dataLen = strnlen(dataStr, 128);
      BmErr res;
      res = spotter_log_console(0, "%.*s", dataLen, dataStr);
      if (res != BmOK) {
        printf("spotter_log_console err: %d\n", res);
      }
    } else if (strncmp("fprintf", command, command_str_len) == 0) {
      const char *filename = FreeRTOS_CLIGetParameter(commandString, 2, &command_str_len);

      if (command_str_len == 0) {
        printf("ERR file name required\n");
        break;
      }
      char *just_filename = static_cast<char *>(pvPortMalloc(command_str_len + 1));
      configASSERT(just_filename);
      memset(just_filename, 0, command_str_len + 1);
      strncpy(just_filename, filename, command_str_len);
      const char *dataStr = FreeRTOS_CLIGetParameter(commandString, 3, &command_str_len);

      if (command_str_len == 0) {
        printf("ERR data required\n");
        vPortFree(just_filename);
        just_filename = NULL;
        break;
      }
      size_t dataLen = strnlen(dataStr, 128);
      BmErr res;
      res = spotter_log(0, just_filename, USE_TIMESTAMP, "%.*s\n", dataLen + 1,
                       dataStr); // add one for the \n
      if (res != BmOK) {
        printf("spotter_log err: %d\n", res);
      }
      vPortFree(just_filename);
      just_filename = NULL;
    } else if (strncmp("txdata", command, command_str_len) == 0) {
      BaseType_t typeStrLen;
      const char *typestr = FreeRTOS_CLIGetParameter(commandString, 2, &typeStrLen);

      if (typeStrLen == 0) {
        printf("Network type required\n");
        break;
      }
      const char *dataStr = FreeRTOS_CLIGetParameter(commandString, 3, &command_str_len);

      if (command_str_len == 0) {
        printf("ERR data required\n");
        break;
      }
      size_t dataLen = strnlen(dataStr, 129);
      if (dataLen > 128) {
        printf("ERR max data len for CLI test is 128 bytes\n");
        break;
      }
      printf("transmit-data byte len: %u\n", (uint8_t)dataLen);
      BmSerialNetworkType type;
      if (strncmp(typestr, "i", typeStrLen) == 0) {
        type = BmNetworkTypeCellularIriFallback;
      } else if (strncmp(typestr, "c", typeStrLen) == 0) {
        type = BmNetworkTypeCellularOnly;
      } else {
        printf("Invalid network type\n");
        break;
      }
      uint8_t *data = static_cast<uint8_t *>(pvPortMalloc(dataLen + 1));
      configASSERT(data);
      memcpy(data, dataStr, dataLen);
      data[dataLen] = 0;
      if (spotter_tx_data(data, dataLen, type)) {
        printf("Sucessfully sent data transmit request\n");
      } else {
        printf("Failed to send data transmit request\n");
      }
      vPortFree(data);
    } else {
      printf("ERR Invalid arguments\n");
      break;
    }
  } while (0);

  return pdFALSE;
}
