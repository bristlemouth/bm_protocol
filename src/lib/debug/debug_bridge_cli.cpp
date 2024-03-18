#include "debug_bridge_cli.h"
#include "device_test_service.h"
#include "device_test_svc_reply_msg.h"
/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"

/* FreeRTOS+CLI includes. */
#include "FreeRTOS_CLI.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static BaseType_t bridgeCliCommand(char *writeBuffer, size_t writeBufferLen,
                                   const char *commandString);
static bool aanderaaTestReply(bool ack, uint32_t msg_id, size_t service_strlen,
                              const char *service, size_t reply_len, uint8_t *reply_data);

static const CLI_Command_Definition_t cmdBridgeCli = {
    // Command string
    "bridge",
    // Help string
    "bridge:\n"
    " * aanderaatest <0xNodeId>\n",
    // Command function
    bridgeCliCommand,
    // Number of parameters (variable)
    -1};

void debugBridgeCliInit() { FreeRTOS_CLIRegisterCommand(&cmdBridgeCli); }

static BaseType_t bridgeCliCommand(char *writeBuffer, size_t writeBufferLen,
                                   const char *commandString) {
  // Remove unused argument warnings
  (void)commandString;
  (void)writeBuffer;
  (void)writeBufferLen;

  do {
    BaseType_t testStrLen;
    const char *testStr = FreeRTOS_CLIGetParameter(commandString, 1, &testStrLen);
    if (testStr == NULL) {
      printf("ERR Invalid paramters\n");
      break;
    }
    if (strncmp(testStr, "aanderaatest", testStrLen) == 0) {
      BaseType_t nodeIdLen;
      const char *nodeIdStr = FreeRTOS_CLIGetParameter(commandString, 2, &nodeIdLen);
      if (nodeIdStr == NULL) {
        printf("ERR Invalid paramters\n");
        break;
      }
      uint64_t node_id = strtoull(nodeIdStr, NULL, 16);
      if (device_test_service_request(node_id, NULL, 0, aanderaaTestReply, 3)) {
        printf("OK\n");
      } else {
        printf("ERR Failed to send request\n");
      }
      break;
    } else {
      printf("ERR Invalid paramters\n");
      break;
    }
  } while (0);

  return pdFALSE;
}

static bool aanderaaTestReply(bool ack, uint32_t msg_id, size_t service_strlen,
                              const char *service, size_t reply_len, uint8_t *reply_data) {
  printf("Received reply for service: %.*s\n", service_strlen, service);
  if (ack) {
    DeviceTestSvcReplyMsg::Data reply;
    if (DeviceTestSvcReplyMsg::decode(reply, reply_data, reply_len)) {
      uint8_t *retries = static_cast<uint8_t *>(reply.data);
      printf("aanderaaTestReply:\n"
             "* MsgId: %u\n"
             "* Success: %d\n"
             "* Retries: %u\n",
             msg_id, reply.success, *retries);
    } else {
      printf("aanderaaTestReply: ERR Failed to decode reply\n");
    }
  } else {
    printf("aanderaaTestReply: ERR Failed to get reply\n");
  }
  return true;
}
