/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"

/* FreeRTOS+CLI includes. */
#include "FreeRTOS_CLI.h"

#include "bm_adin2111.h"
#include "bm_config.h"
#include "debug.h"
#include "debug_adin_raw.h"
#include "util.h"
#include <string.h>

static BaseType_t adinCommand(char *writeBuffer, size_t writeBufferLen,
                              const char *commandString);

static const CLI_Command_Definition_t cmdGpio = {
    // Command string
    "adin",
    // Help string
    "adin:\n"
    " * adin init\n"
    " * adin tx <port> <data>\n",
    // Command function
    adinCommand,
    // Number of parameters (variable)
    -1};

void debugAdinRawInit(void) { FreeRTOS_CLIRegisterCommand(&cmdGpio); }

BmErr debug_l2_rx(void *device_handle, uint8_t *payload, uint16_t payload_len,
                  uint8_t port_mask) {
  (void)device_handle;

  printf("ADIN RX <%d> ", port_mask);
  for (uint32_t idx = 0; idx < payload_len; idx++) {
    printf("%02X ", payload[idx]);
  }
  printf("\n");

  return BmOK;
}

uint8_t data[] = {0x33, 0x33, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0xD5, 0xAD, 0x7A, 0xDD, 0x86,
                  0xDD, 0x60, 0x00, 0x00, 0x00, 0x00, 0x20, 0x11, 0xFF, 0x20, 0x01, 0x0D, 0xB8,
                  0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0xD5, 0xFF, 0xFE, 0xAD, 0x7A, 0xDD, 0xFF,
                  0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                  0x00, 0x01, 0x08, 0xAE, 0x08, 0xAE, 0x00, 0x20, 0x06, 0x1E, 0x00, 0x00, 0x07,
                  0x00, 0x9F, 0x9F, 0x1A, 0xB8, 0x0D, 0x01, 0x20, 0x00, 0x1A, 0xFF, 0xD5, 0x00,
                  0x00, 0x1A, 0xDD, 0x7A, 0xAD, 0xFE, 0xFF, 0xFF};

static BaseType_t adinCommand(char *writeBuffer, size_t writeBufferLen,
                              const char *commandString) {
  // Remove unused argument warnings
  (void)commandString;
  (void)writeBuffer;
  (void)writeBufferLen;

  do {
    const char *parameter;
    BaseType_t parameterStringLength;
    parameter = FreeRTOS_CLIGetParameter(commandString,
                                         1, // Get the first parameter (command)
                                         &parameterStringLength);

    if (parameter == NULL) {
      printf("ERR Invalid paramters\n");
      break;
    }

    if (strncmp("init", parameter, parameterStringLength) == 0) {
      BmErr err = adin2111_init();
      if (err == BmEALREADY) {
        printf("Adin already initialized\n");
      } else if (err == BmOK) {
        printf("Adin initialized successfully\n");
      } else {
        printf("Adin initialization failed :(\n");
      }
    } else if (strncmp("tx", parameter, parameterStringLength) == 0) {
      NetworkDevice device = adin2111_network_device();
      const uint8_t all_ports_mask = 3;
      BmErr err = device.trait->send(device.self, data, sizeof(data), all_ports_mask);
      if (err == BmOK) {
        printf("OK!\n");
      } else {
        printf("ERR %d\n", err);
      }
    } else {
      printf("ERR Invalid paramters\n");
      break;
    }
  } while (0);

  return pdFALSE;
}
