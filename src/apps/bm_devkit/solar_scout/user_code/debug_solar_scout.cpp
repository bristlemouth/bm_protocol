#include "FreeRTOS.h"
#include "FreeRTOS_CLI.h"
#include "bm_config.h"
#include "mbedtls_base64/base64.h"
#include "serial_console.h"
#include "solar_scout_comms.h"
#include "stdlib.h"
#include <string.h>

static BaseType_t cmd_sss_cb(char *write_buffer, size_t write_len, const char *command_string) {
  (void)write_buffer;
  (void)write_len;

  BaseType_t parameter_str_len = 0;
  const char *sss_cmd_str = NULL;

  sss_cmd_str = FreeRTOS_CLIGetParameter(command_string, 1, &parameter_str_len);

  if (!parameter_str_len || !sss_cmd_str) {
    bm_debug("Invalid parameters, please ensure <cmd> is set.\n");
    return pdFALSE;
  }

  SSSCommand cmd = (SSSCommand)strtoul(sss_cmd_str, NULL, 0);
  const char *message = FreeRTOS_CLIGetParameter(command_string, 2, &parameter_str_len);
  static uint8_t payload[CONSOLE_RX_BUFF_SIZE] = {0};
  size_t decode_len = 0;

  if (message != NULL) {
    if (mbedtls_base64_decode(payload, sizeof(payload), &decode_len,
                              (const unsigned char *)message, parameter_str_len) != 0) {
      bm_debug("Could not decode Base64 string.\n");
      return pdFALSE;
    }
  }

  BmErr err = solar_scout_send_cmd(cmd, payload, (uint16_t)decode_len);
  if (err == BmOK) {
    bm_debug("Sending cmd: %d", cmd);
    if (message) {
      bm_debug(", data: %.*s", (int)parameter_str_len, message);
    }
    bm_debug("\n");
  } else {
    bm_debug("Failed to send message.\n");
  }

  return pdFALSE;
}

static const CLI_Command_Definition_t cmd_sss = {
    // Command string
    "sss",
    // Help string
    "sss:\n"
    " * sss <cmd> <base64_data>\n",
    // Command function
    cmd_sss_cb,
    // Number of parameters
    -1,
};

void debug_solar_scout_init() { FreeRTOS_CLIRegisterCommand(&cmd_sss); }
