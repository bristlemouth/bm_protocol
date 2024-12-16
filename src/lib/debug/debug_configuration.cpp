/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"

/* FreeRTOS+CLI includes. */
#include "FreeRTOS_CLI.h"

#include "app_util.h"
#include "debug_configuration.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static BaseType_t configurationCommand(char *writeBuffer, size_t writeBufferLen,
                                       const char *commandString);

static const CLI_Command_Definition_t cmdConfiguration = {
    // Command string
    "cfg",
    // Help string
    "cfg:\n"
    " * cfg <usr/hw/sys> set <key> <type:uint,int,float,str,bytestr> <value>\n"
    " * cfg <usr/hw/sys> get <key> <type>\n"
    " * cfg <usr/hw/sys> del <key>\n"
    " * cfg <usr/hw/sys> save\n"
    " * cfg <usr/hw/sys> listkeys\n",
    // Command function
    configurationCommand,
    // Number of parameters (variable)
    -1};

void debugConfigurationInit(void) { FreeRTOS_CLIRegisterCommand(&cmdConfiguration); }

static BaseType_t configurationCommand(char *writeBuffer, size_t writeBufferLen,
                                       const char *commandString) {
  const char *parameter;
  BaseType_t parameterStringLength;

  // Remove unused argument warnings
  (void)commandString;
  (void)writeBuffer;
  (void)writeBufferLen;

  do {
    BaseType_t partitionStrLen;
    const char *partitionStr = FreeRTOS_CLIGetParameter(commandString, 1, &partitionStrLen);
    if (partitionStr == NULL) {
      printf("ERR Invalid paramters\n");
      break;
    }
    BmConfigPartition partition;
    if (strncmp("usr", partitionStr, partitionStrLen) == 0) {
      partition = BM_CFG_PARTITION_USER;
    } else if (strncmp("hw", partitionStr, partitionStrLen) == 0) {
      partition = BM_CFG_PARTITION_HARDWARE;
    } else if (strncmp("sys", partitionStr, partitionStrLen) == 0) {
      partition = BM_CFG_PARTITION_SYSTEM;
    } else {
      printf("ERR Invalid paramters\n");
      break;
    }

    parameter = FreeRTOS_CLIGetParameter(commandString, 2, &parameterStringLength);

    if (parameter == NULL) {
      printf("ERR Invalid paramters\n");
      break;
    }

    if (strncmp("set", parameter, parameterStringLength) == 0) {
      BaseType_t keyStrLen;
      const char *keystr = FreeRTOS_CLIGetParameter(commandString, 3, &keyStrLen);
      if (keystr == NULL) {
        printf("ERR Invalid paramters\n");
        break;
      }
      BaseType_t typeStrLen;
      const char *typestr = FreeRTOS_CLIGetParameter(commandString, 4, &typeStrLen);
      if (typestr == NULL) {
        printf("ERR Invalid paramters\n");
        break;
      }
      BaseType_t valueStrLen;
      const char *valuestr = FreeRTOS_CLIGetParameter(commandString, 5, &valueStrLen);
      if (valuestr == NULL) {
        printf("ERR Invalid paramters\n");
        break;
      }
      if (strncmp("uint", typestr, typeStrLen) == 0) {
        char *ptr;
        uint32_t value = strtol(valuestr, &ptr, 10);
        if (set_config_uint(partition, keystr, keyStrLen, value)) {
          printf("set %lu\n", value);
        } else {
          printf("failed to set %lu\n", value);
        }
      } else if (strncmp("int", typestr, typeStrLen) == 0) {
        char *ptr;
        int32_t value = strtol(valuestr, &ptr, 10);
        if (set_config_int(partition, keystr, keyStrLen, value)) {
          printf("set %ld\n", value);
        } else {
          printf("failed to set %ld\n", value);
        }
      } else if (strncmp("float", typestr, typeStrLen) == 0) {
        float value;
        if (!bStrtof((char *)valuestr, &value)) {
          printf("ERR Invalid paramters\n");
          break;
        }
        if (set_config_float(partition, keystr, keyStrLen, value)) {
          printf("set %f\n", value);
        } else {
          printf("failed to set %fd\n", value);
        }
      } else if (strncmp("str", typestr, typeStrLen) == 0) {
        if (set_config_string(partition, keystr, keyStrLen, valuestr, valueStrLen)) {
          printf("set %s\n", valuestr);
        } else {
          printf("failed to set %sd\n", valuestr);
        }
      } else if (strncmp("bytestr", typestr, typeStrLen) == 0) {
        if (set_config_buffer(partition, keystr, keyStrLen, (uint8_t *)valuestr,
                              strlen(valuestr) + 1)) {
          printf("set %s\n", valuestr);
        } else {
          printf("failed to set %sd\n", valuestr);
        }
      } else {
        printf("ERR Invalid paramters\n");
        break;
      }
    } else if (strncmp("get", parameter, parameterStringLength) == 0) {
      BaseType_t keyStrLen;
      const char *keystr = FreeRTOS_CLIGetParameter(commandString, 3, &keyStrLen);
      if (keystr == NULL) {
        printf("ERR Invalid paramters\n");
        break;
      }
      BaseType_t typeStrLen;
      const char *typestr = FreeRTOS_CLIGetParameter(commandString, 4, &typeStrLen);
      if (typestr == NULL) {
        printf("ERR Invalid paramters\n");
        break;
      }
      if (strncmp("uint", typestr, typeStrLen) == 0) {
        uint32_t value;
        if (get_config_uint(partition, keystr, keyStrLen, &value)) {
          printf("get %lu\n", value);
        } else {
          printf("failed to get %s\n", keystr);
        }
      } else if (strncmp("int", typestr, typeStrLen) == 0) {
        int32_t value;
        if (get_config_int(partition, keystr, keyStrLen, &value)) {
          printf("get %ld\n", value);
        } else {
          printf("failed to get %s\n", keystr);
        }
      } else if (strncmp("float", typestr, typeStrLen) == 0) {
        float value;
        if (get_config_float(partition, keystr, keyStrLen, &value)) {
          printf("get %f\n", value);
        } else {
          printf("failed to get %s\n", keystr);
        }
      } else if (strncmp("str", typestr, typeStrLen) == 0) {
        char strbuf[MAX_CONFIG_BUFFER_SIZE_BYTES];
        size_t strlen = sizeof(strbuf);
        if (get_config_string(partition, keystr, keyStrLen, strbuf, &strlen)) {
          strbuf[strlen] = '\0';
          printf("get %s\n", strbuf);
        } else {
          printf("failed to get %s\n", keystr);
        }
      } else if (strncmp("bytestr", typestr, typeStrLen) == 0) {
        uint8_t bytes[MAX_CONFIG_BUFFER_SIZE_BYTES];
        size_t bytelen = sizeof(bytes);
        if (get_config_buffer(partition, keystr, keyStrLen, bytes, &bytelen)) {
          printf("get bytes:");
          for (size_t i = 0; i < bytelen; i++) {
            printf("%02x:", bytes[i]);
          }
          printf("\n");
        } else {
          printf("failed to get %s\n", keystr);
        }
      } else {
        printf("ERR Invalid paramters\n");
        break;
      }

    } else if (strncmp("listkeys", parameter, parameterStringLength) == 0) {
      uint8_t num_keys = 0;
      const ConfigKey *keys = get_stored_keys(partition, &num_keys);
      printf("num keys: %u\n", num_keys);
      for (int i = 0; i < num_keys; i++) {
        size_t keybufSize = keys[i].key_len + 1;
        char *keybuf = (char *)pvPortMalloc(keybufSize);
        configASSERT(keybuf);
        memcpy(keybuf, keys[i].key_buf, keybufSize);
        keybuf[keys[i].key_len] = '\0';
        printf("%s : %s\n", keybuf, data_type_enum_to_str(keys[i].value_type));
        vPortFree(keybuf);
      }
    } else if (strncmp("del", parameter, parameterStringLength) == 0) {
      BaseType_t keyStrLen;
      const char *keystr = FreeRTOS_CLIGetParameter(commandString, 3, &keyStrLen);
      if (keystr == NULL) {
        printf("ERR Invalid paramters\n");
        break;
      }
      if (remove_key(partition, keystr, keyStrLen)) {
        printf("Removed key %s\n", keystr);
      } else {
        printf("Failed to remove key %s\n", keystr);
      }
    } else if (strncmp("save", parameter, parameterStringLength) == 0) {
      if (!save_config(partition, true)) {
        printf("Failed to save config.\n");
      }
      // Succesfull "save" will reset the system.
    } else {
      printf("ERR Invalid paramters\n");
      break;
    }
  } while (0);

  return pdFALSE;
}
