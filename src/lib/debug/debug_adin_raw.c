/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"

/* FreeRTOS+CLI includes. */
#include "FreeRTOS_CLI.h"

#include <string.h>
#include "debug_adin_raw.h"
#include "eth_adin2111.h"
#include "debug.h"

static BaseType_t adinCommand( char *writeBuffer,
                  size_t writeBufferLen,
                  const char *commandString);

static const CLI_Command_Definition_t cmdGpio = {
  // Command string
  "adin",
  // Help string
  "adin:\n"
  "init\n"
  "tx <port> <data>\n",
  // Command function
  adinCommand,
  // Number of parameters (variable)
  -1
};

static adin2111_DeviceStruct_t _device;

void debugAdinRawInit(void) {
  FreeRTOS_CLIRegisterCommand( &cmdGpio );
}

int8_t debug_l2_rx(void* device_handle, uint8_t* payload, uint16_t payload_len, uint8_t port_mask) {
  (void)device_handle;

  printf("ADIN RX <%d> ", port_mask);
  for(uint32_t idx = 0; idx < payload_len; idx++){
    printf("%02X ", payload[idx]);
  }
  printf("\n");

  return ERR_OK;
}

static bool _adin_started;
uint8_t data[] = {0x33,0x33,0x00,0x00,0x00,0x01,0x00,0x00,0xD5,0xAD,0x7A,0xDD,0x86,0xDD,0x60,0x00,0x00,0x00,0x00,0x20,0x11,0xFF,0x20,0x01,0x0D,0xB8,0x02,0x00,0x00,0x00,0x00,0x00,0xD5,0xFF,0xFE,0xAD,0x7A,0xDD,0xFF,0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x08,0xAE,0x08,0xAE,0x00,0x20,0x06,0x1E,0x00,0x00,0x07,0x00,0x9F,0x9F,0x1A,0xB8,0x0D,0x01,0x20,0x00,0x1A,0xFF,0xD5,0x00,0x00,0x1A,0xDD,0x7A,0xAD,0xFE,0xFF,0xFF};

static BaseType_t adinCommand( char *writeBuffer,
                  size_t writeBufferLen,
                  const char *commandString) {
  // Remove unused argument warnings
  ( void ) commandString;
  ( void ) writeBuffer;
  ( void ) writeBufferLen;

  do {
    const char *parameter;
    BaseType_t parameterStringLength;
    parameter = FreeRTOS_CLIGetParameter(
          commandString,
          1, // Get the first parameter (command)
          &parameterStringLength);

    if(parameter == NULL) {
      printf("ERR Invalid paramters\n");
      break;
    }

    if (strncmp("init", parameter,parameterStringLength) == 0) {
      if(_adin_started) {
        printf("Adin already initialized\n");
      } else if(adin2111_hw_init(&_device, debug_l2_rx) == ADI_ETH_SUCCESS) {
        printf("Adin initialized successfully\n");
        _adin_started = true;
      } else {
        printf("Adin initialization failed :(\n");
      }
    } else if (strncmp("tx", parameter,parameterStringLength) == 0) {

      int8_t rval = adin2111_tx(&_device, data, sizeof(data), 0x3, 0);
      if(rval) {
        printf("ERR %d\n", rval);
      } else {
        printf("OK!\n");
      }
    } else {
      printf("ERR Invalid paramters\n");
      break;
    }
  } while(0);

  return pdFALSE;
}
