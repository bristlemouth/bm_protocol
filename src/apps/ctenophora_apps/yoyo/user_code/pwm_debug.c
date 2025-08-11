#include "FreeRTOS.h"
#include "FreeRTOS_CLI.h"
#include "motor_process.h"
#include <stdlib.h>
#include <string.h>

static BaseType_t pwm_command(char *buf, size_t len, const char *command);

static const CLI_Command_Definition_t pwm_configuration = {
    // Command string
    "pwm",
    // Help string
    "pwm:\n"
    " * set <type> <value>\n"
    " * get <type>\n",
    // Command function
    pwm_command,
    // Number of parameters (variable)
    -1,
};

static BaseType_t pwm_set_command(const char *command) {
  BaseType_t ret = pdFAIL;
  BaseType_t type_len = 0;
  const char *type_str = FreeRTOS_CLIGetParameter(command, 2, &type_len);
  BaseType_t duty_len = 0;
  const char *duty_str = FreeRTOS_CLIGetParameter(command, 3, &duty_len);

  if (duty_str && type_str) {
    uint32_t select = strtol(type_str, NULL, 10);
    uint32_t duty = strtol(duty_str, NULL, 10);
    IOPinHandle_t *handle = NULL;
    switch (select) {
    case 1:
      handle = &MOTOR_SPEED1;
      break;
    case 2:
      handle = &MOTOR_SPEED2;
      break;
    default:
      break;
    }

    if (handle) {
      set_pwm_duty(duty, handle);
      ret = pdPASS;
    }
  }

  return ret;
}

static BaseType_t pwm_command(char *write_buf, size_t write_len, const char *command) {
  (void)write_buf;
  (void)write_len;
  BaseType_t command_type_len = 0;
  const char *command_type_str = FreeRTOS_CLIGetParameter(command, 1, &command_type_len);

  if (!command_type_str) {
    return pdFAIL;
  }

  if (command_type_str && strncmp("set", command_type_str, command_type_len) == 0) {
    return pwm_set_command(command);
  } else if (command_type_str && strncmp("get", command_type_str, command_type_len) == 0) {
    return pdPASS;
  }

  return pdFAIL;
}

void pwm_debug_init(void) { FreeRTOS_CLIRegisterCommand(&pwm_configuration); }
