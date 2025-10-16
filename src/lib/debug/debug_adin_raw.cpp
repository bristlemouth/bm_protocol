/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"

/* FreeRTOS+CLI includes. */
#include "FreeRTOS_CLI.h"

#include "adi_hal.h"
#include "bm_adin2111.h"
#include "bm_ports.h"
#include "bristlemouth_client.h"
#include "bsp.h"
#include "debug_adin_raw.h"
#include "l2.h"
#include "stress.h"
#include "util.h"
#include <string.h>

extern "C" {
#include "bm_ip.h"
}

extern adin_pins_t adin_pins;

static bool network_device_interrupt(const void *pinHandle, uint8_t value, void *args) {
  (void)pinHandle;
  (void)value;
  (void)args;
  return bm_l2_handle_device_interrupt() == BmOK;
}

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

void debug_l2_rx(uint8_t port_mask, uint8_t *payload, size_t payload_len) {

  printf("ADIN RX <%d> ", port_mask);
  for (uint32_t idx = 0; idx < payload_len; idx++) {
    printf("%02X ", payload[idx]);
  }
  printf("\n");
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
  static NetworkDevice network_device = {};

  do {
    const char *parameter;
    BaseType_t parameterStringLength;
    parameter = FreeRTOS_CLIGetParameter(commandString,
                                         1, // Get the first parameter (command)
                                         &parameterStringLength);
    static bool stack_initialized = false;

    if (parameter == NULL) {
      printf("ERR Invalid paramters\n");
      break;
    }

    if (strncmp("init", parameter, parameterStringLength) == 0) {
      IORegisterCallback(adin_pins.interrupt, network_device_interrupt, NULL);
      HAL_Init_Hook();
      network_device = adin2111_network_device();
      network_device.callbacks->power = bcl_power_callback;
      BmErr err = adin2111_init();
      if (err == BmOK) {
        printf("Adin initialized successfully\n");
        if (bm_l2_init(network_device) == BmOK) {
          network_device.callbacks->receive = debug_l2_rx;
          printf("L2 initialized successfully\n");
        } else {
          printf("L2 initialization failed, err: %d\n", err);
        }

        if (!stack_initialized) {
          bm_ip_init();
          stack_initialized = true;
        }
        stress_test_init(network_device, STRESS_TEST_PORT);
      } else {
        printf("Adin initialization failed, err: %d :(\n", err);
      }
    } else if (strncmp("off", parameter, parameterStringLength) == 0) {
      bcl_power_callback(false);
      network_device = (NetworkDevice){};
      bm_l2_deinit();
      stress_test_deinit();
    } else if (strncmp("tx", parameter, parameterStringLength) == 0) {
      const uint8_t all_ports_mask = 3;

      if (!network_device.trait) {
        printf("Adin not initialized.\n");
        break;
      }

      BmErr err =
          network_device.trait->send(network_device.self, data, sizeof(data), all_ports_mask);
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
