#include "debug_bm_service.h"
/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"

/* FreeRTOS+CLI includes. */
#include "FreeRTOS_CLI.h"

#include "bm_service.h"
#include "bm_service_request.h"
#include "sys_info_service.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static BaseType_t bmServiceCmd(char *writeBuffer, size_t writeBufferLen,
                               const char *commandString);

static const CLI_Command_Definition_t cmdBmService = {
    // Command string
    "bmsrv",
    // Help string
    "bmsrv:\n"
    " * bmsrv req <service> <data> <timeout s>\n"
    " * bmsrv req sysinfo <node id> <timeout s>\n",
    // Command function
    bmServiceCmd,
    // Number of parameters (variable)
    -1};

void debugBmServiceInit(void) { FreeRTOS_CLIRegisterCommand(&cmdBmService); }

static bool reply_cb(bool ack, uint32_t msg_id, size_t service_strlen, const char *service,
                     size_t reply_len, uint8_t *reply_data) {
  printf("Msg id: %" PRIu32 "\n", msg_id);
  if (ack) {
    printf("Service: %.*s\n", service_strlen, service);
    printf("Reply: ");
    for (size_t i = 0; i < reply_len; i++) {
      printf("%02x ", reply_data[i]);
      if (i % 16 == 15) {
        printf("\n");
      }
    }
    printf("\n");
  } else {
    printf("NACK\n");
  }
  return true;
}

static bool sys_info_reply_cb(bool ack, uint32_t msg_id, size_t service_strlen, const char *service,
                         size_t reply_len, uint8_t *reply_data) {
  printf("Msg id: %" PRIu32 "\n", msg_id);
  if (ack && reply_len >= sizeof(sys_info_service_data_s)) {
    printf("Service: %.*s\n", service_strlen, service);
    printf("Reply: \n");
    sys_info_service_data_s *reply = (sys_info_service_data_s *)reply_data;
    printf(" * Node id: %" PRIx64 "\n", reply->node_id);
    printf(" * Git SHA: 0x%08" PRIx32 "\n", reply->gitSHA);
    printf(" * Sys config CRC: 0x%08" PRIx32 "\n", reply->sys_config_crc);
    printf(" * App name: %.*s\n", reply->app_name_strlen, reply->app_name);
  } else {
    printf("NACK\n");
  }
  return true;
}

static BaseType_t bmServiceCmd(char *writeBuffer, size_t writeBufferLen,
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

    if (strncmp("req", parameter, parameterStringLength) == 0) {
      BaseType_t serviceStrLen;
      const char *serviceStr = FreeRTOS_CLIGetParameter(commandString,
                                                        2, // Get the second parameter (service)
                                                        &serviceStrLen);
      if (serviceStr == NULL) {
        printf("ERR Invalid paramters\n");
        break;
      }
      if (strncmp("sysinfo", serviceStr, serviceStrLen) == 0) {
        BaseType_t nodeStrLen;
        const char *nodeStr = FreeRTOS_CLIGetParameter(commandString,
                                                       3, // Get the third parameter (node id)
                                                       &nodeStrLen);
        if (nodeStr == NULL) {
          printf("ERR Invalid paramters\n");
          break;
        }
        uint64_t nodeId = strtoull(nodeStr, NULL, 16);
        BaseType_t timeoutStrLen;
        const char *timeoutStr =
            FreeRTOS_CLIGetParameter(commandString,
                                     4, // Get the fourth parameter (timeout)
                                     &timeoutStrLen);
        if (timeoutStr == NULL) {
          printf("ERR Invalid paramters\n");
          break;
        }
        uint32_t timeout_s = strtoul(timeoutStr, NULL, 10);
        if (sys_info_service_request(nodeId, sys_info_reply_cb, timeout_s)) {
          printf("OK\n");
        } else {
          printf("ERR\n");
        }
        break;
      } else {
        BaseType_t dataStrLen;
        const char *dataStr = FreeRTOS_CLIGetParameter(commandString,
                                                       3, // Get the third parameter (data)
                                                       &dataStrLen);
        if (dataStr == NULL) {
          printf("ERR Invalid paramters\n");
          break;
        }
        BaseType_t timeoutStrLen;
        const char *timeoutStr =
            FreeRTOS_CLIGetParameter(commandString,
                                     4, // Get the fourth parameter (timeout)
                                     &timeoutStrLen);
        if (timeoutStr == NULL) {
          printf("ERR Invalid paramters\n");
          break;
        }
        uint32_t timeoutMs = strtoul(timeoutStr, NULL, 10);
        if (bm_service_request(serviceStrLen, serviceStr, dataStrLen, (const uint8_t *)dataStr,
                               reply_cb, timeoutMs)) {
          printf("OK\n");
        } else {
          printf("ERR\n");
        }
      }
    } else {
      printf("ERR Invalid paramters\n");
      break;
    }
  } while (0);

  return pdFALSE;
}
