#include "FreeRTOS.h"
#include "task.h"
#include <string.h>

// FreeRTOS+CLI includes.
#include "FreeRTOS_CLI.h"

#include "bcmp.h"

#include "configuration.h"

#include "app_util.h"
#include "messages.h"
#include "pubsub.h"
extern "C" {
#include "messages/config.h"
#include "messages/heartbeat.h"
#include "messages/info.h"
#include "messages/neighbors.h"
#include "messages/ping.h"
#include "messages/resource_discovery.h"
#include "messages/time.h"
#include "topology.h"
}
#include "util.h"

#include "debug.h"

static BaseType_t cmd_bcmp_fn(char *writeBuffer, size_t writeBufferLen,
                              const char *commandString);
static const CLI_Command_Definition_t cmd_bcmp = {
    // Command string
    "bm",
    // Help string
    "bm:\n"
    " * bm neighbors\n"
    " * bm info <node_id>\n"
    " * bm ping <node_id>\n"
    " * bm cfg get <node_id> <partition(u/s)> <key>\n"
    " * bm cfg set <node_id> <partition(u/s)> <type(u/i/f/s/b)> <key> <value>\n"
    " * bm cfg commit <node_id> <partition(u/s)>\n"
    " * bm cfg status <node_id> <partition(u/s)>\n"
    " * bm cfg del <node_id> <partition(u/s)> <key>\n"
    " * bm time set <node_id> <utc_us>\n"
    " * bm time get <node_id>\n"
    " * bm topo\n"
    " * bm resources\n"
    " * bm resources <node_id>\n"
    " * bm sub <topic>\n"
    " * bm unsub <topic>\n"
    " * bm pub <topic> <data> <type> <version>\n",
    // Command function
    cmd_bcmp_fn,
    // Number of parameters
    -1};

static void print_subscriptions(uint64_t node_id, const char *topic, uint16_t topic_len,
                                const uint8_t *data, uint16_t data_len, uint8_t type,
                                uint8_t version) {
  (void)node_id;

  printf("BM pubsub version: %u\n", version);
  printf("BM pubsub type: %u\n", type);
  printf("Received %u bytes of data on topic: %.*s\n", data_len, topic_len, topic);
  printf("Data: ");
  for (size_t i = 0; i < data_len; ++i)
    printf("%02X ", (unsigned char)data[i]);
  printf("\n");
}

void print_neighbor_basic(BcmpNeighbor *neighbor) {
  printf("%016" PRIx64 " |   %u  | %7s | %0.3f\n", neighbor->node_id, neighbor->port,
         neighbor->online ? "online" : "offline",
         (float)((xTaskGetTickCount() - neighbor->last_heartbeat_ticks)) / 1000.0);
}

static BaseType_t cmd_bcmp_fn(char *writeBuffer, size_t writeBufferLen,
                              const char *commandString) {
  // Remove unused argument warnings
  (void)writeBuffer;
  (void)writeBufferLen;
  (void)commandString;

  do {
    const char *command;
    BaseType_t command_str_len;
    command = FreeRTOS_CLIGetParameter(commandString, 1, &command_str_len);

    if (command == NULL) {
      printf("ERR Invalid command\n");
      break;
    } else if (strncmp("neighbors", command, command_str_len) == 0) {
      printf("    Node ID      | Port |  State  | Time since last heartbeat (s)\n");
      bcmp_neighbor_foreach(print_neighbor_basic);
    } else if (strncmp("info", command, command_str_len) == 0) {
      const char *node_id_str;
      BaseType_t node_id_str_len = 0;
      node_id_str = FreeRTOS_CLIGetParameter(commandString, 2, &node_id_str_len);

      if (node_id_str_len > 0) {
        uint64_t node_id = strtoull(node_id_str, NULL, 16);

        BcmpNeighbor *neighbor = bcmp_find_neighbor(node_id);
        if (neighbor) {
          bcmp_print_neighbor_info(neighbor);
        } else {
          printf("Device not in neighbor table. Sending global request.\n");
          bcmp_expect_info_from_node_id(node_id);
          if (bcmp_request_info(node_id, &multicast_global_addr, NULL) != BmOK) {
            printf("Error sending request\n");
          }
        }
      } else {
        printf("Invalid arguments\n");
      }
    } else if (strncmp("ping", command, command_str_len) == 0) {
      const char *node_id_str;
      const char *ping_message;
      BaseType_t node_id_str_len = 0;
      BaseType_t ping_message_length = 0;
      node_id_str = FreeRTOS_CLIGetParameter(commandString, 2, &node_id_str_len);
      if (node_id_str_len > 0) {
        uint64_t node_id = strtoull(node_id_str, NULL, 16);
        ping_message = FreeRTOS_CLIGetParameter(commandString, 3, &ping_message_length);
        if (bcmp_send_ping_request(node_id, &multicast_global_addr,
                                   (const uint8_t *)ping_message,
                                   ping_message_length) != BmOK) {
          printf("Error sending ping\n");
        }
      } else {
        printf("Invalid node_id\n");
      }
    } else if (strncmp("cfg", command, command_str_len) == 0) {
      const char *cmd_id_str;
      BaseType_t cmd_id_str_len = 0;
      cmd_id_str = FreeRTOS_CLIGetParameter(commandString, 2, &cmd_id_str_len);
      if (!cmd_id_str) {
        printf("Invalid arguments\n");
        break;
      }
      if (strncmp("get", cmd_id_str, cmd_id_str_len) == 0) {
        const char *node_id_str;
        BaseType_t node_id_str_len = 0;
        node_id_str = FreeRTOS_CLIGetParameter(commandString, 3, &node_id_str_len);
        const char *part_str;
        BaseType_t part_str_len = 0;
        part_str = FreeRTOS_CLIGetParameter(commandString, 4, &part_str_len);
        const char *key_str;
        BaseType_t key_str_str_len = 0;
        key_str = FreeRTOS_CLIGetParameter(commandString, 5, &key_str_str_len);
        if (!key_str || !part_str || !node_id_str) {
          printf("Invalid arguments\n");
          break;
        }
        uint64_t node_id = strtoull(node_id_str, NULL, 16);
        BmConfigPartition partition;
        if (strncmp("u", part_str, part_str_len) == 0) {
          partition = BM_CFG_PARTITION_USER;
        } else if (strncmp("s", part_str, part_str_len) == 0) {
          partition = BM_CFG_PARTITION_SYSTEM;
        } else {
          printf("Invalid arguments\n");
          break;
        }
        BmErr err;
        if (!bcmp_config_get(node_id, partition, key_str_str_len, key_str, &err, NULL)) {
          printf("Failed to send message config get\n");
        } else {
          printf("Succesfully sent config get msg\n");
        }
      } else if (strncmp("set", cmd_id_str, cmd_id_str_len) == 0) {
        const char *node_id_str;
        BaseType_t node_id_str_len = 0;
        node_id_str = FreeRTOS_CLIGetParameter(commandString, 3, &node_id_str_len);
        const char *part_str;
        BaseType_t part_str_len = 0;
        part_str = FreeRTOS_CLIGetParameter(commandString, 4, &part_str_len);
        const char *type_str;
        BaseType_t type_str_str_len = 0;
        type_str = FreeRTOS_CLIGetParameter(commandString, 5, &type_str_str_len);
        const char *key_str;
        BaseType_t key_str_str_len = 0;
        key_str = FreeRTOS_CLIGetParameter(commandString, 6, &key_str_str_len);
        const char *value_str;
        BaseType_t value_str_str_len = 0;
        value_str = FreeRTOS_CLIGetParameter(commandString, 7, &value_str_str_len);
        if (!key_str || !part_str || !type_str || !node_id_str || !value_str) {
          printf("Invalid arguments\n");
          break;
        }
        uint64_t node_id = strtoull(node_id_str, NULL, 16);
        BmConfigPartition partition;
        if (strncmp("u", part_str, part_str_len) == 0) {
          partition = BM_CFG_PARTITION_USER;
        } else if (strncmp("s", part_str, part_str_len) == 0) {
          partition = BM_CFG_PARTITION_SYSTEM;
        } else {
          printf("Invalid arguments\n");
          break;
        }
        ConfigDataTypes type;
        if (strncmp("u", type_str, type_str_str_len) == 0) {
          type = UINT32;
        } else if (strncmp("i", type_str, type_str_str_len) == 0) {
          type = INT32;
        } else if (strncmp("f", type_str, type_str_str_len) == 0) {
          type = FLOAT;
        } else if (strncmp("s", type_str, type_str_str_len) == 0) {
          type = STR;
        } else if (strncmp("b", type_str, type_str_str_len) == 0) {
          type = BYTES;
        } else {
          printf("Invalid arguments\n");
          break;
        }
        BmErr err;
        size_t buffer_size = MAX_STR_LEN_BYTES;
        uint8_t *cbor_buf = static_cast<uint8_t *>(pvPortMalloc(buffer_size));
        configASSERT(cbor_buf);
        memset(cbor_buf, 0, MAX_STR_LEN_BYTES);
        CborEncoder encoder;
        cbor_encoder_init(&encoder, cbor_buf, buffer_size, 0);
        switch (type) {
        case UINT32: {
          uint32_t val = strtoul(value_str, NULL, 0);
          if (cbor_encode_uint(&encoder, val) != CborNoError) {
            printf("Failed to encode uint message...");
          }
          break;
        }
        case INT32: {
          int32_t val = strtol(value_str, NULL, 0);
          if (cbor_encode_int(&encoder, val) != CborNoError) {
            printf("Failed to encode int message...");
          }
          break;
        }
        case FLOAT: {
          float val = strtof(const_cast<char *>(value_str), NULL);
          if (cbor_encode_float(&encoder, val) != CborNoError) {
            printf("Failed to encode float message...");
          }
          break;
        }
        case STR: {
          if (cbor_encode_text_stringz(&encoder, value_str) != CborNoError) {
            printf("Failed to encode string message...");
          }
          break;
        }
        case BYTES: {
          if (cbor_encode_byte_string(&encoder, reinterpret_cast<const uint8_t *>(value_str),
                                      strlen(value_str)) != CborNoError) {
            printf("Failed to encode byte message...");
          }
          break;
        }
        default:
          break;
        }
        if (!bcmp_config_set(node_id, partition, key_str_str_len, key_str, buffer_size,
                             cbor_buf, &err, NULL)) {
          printf("Failed to send message config set\n");
        } else {
          printf("Succesfully sent config set msg\n");
        }
        vPortFree(cbor_buf);
      } else if (strncmp("commit", cmd_id_str, cmd_id_str_len) == 0) {
        const char *node_id_str;
        BaseType_t node_id_str_len = 0;
        node_id_str = FreeRTOS_CLIGetParameter(commandString, 3, &node_id_str_len);
        const char *part_str;
        BaseType_t part_str_len = 0;
        part_str = FreeRTOS_CLIGetParameter(commandString, 4, &part_str_len);
        if (!part_str || !node_id_str) {
          printf("Invalid arguments\n");
          break;
        }
        uint64_t node_id = strtoull(node_id_str, NULL, 16);
        BmConfigPartition partition;
        if (strncmp("u", part_str, part_str_len) == 0) {
          partition = BM_CFG_PARTITION_USER;
        } else if (strncmp("s", part_str, part_str_len) == 0) {
          partition = BM_CFG_PARTITION_SYSTEM;
        } else {
          printf("Invalid arguments\n");
          break;
        }
        BmErr err;
        if (!bcmp_config_commit(node_id, partition, &err)) {
          printf("Failed to send config commit\n");
        } else {
          printf("Succesfully config commit send\n");
        }
      } else if (strncmp("status", cmd_id_str, cmd_id_str_len) == 0) {
        const char *node_id_str;
        BaseType_t node_id_str_len = 0;
        node_id_str = FreeRTOS_CLIGetParameter(commandString, 3, &node_id_str_len);
        const char *part_str;
        BaseType_t part_str_len = 0;
        part_str = FreeRTOS_CLIGetParameter(commandString, 4, &part_str_len);
        if (!part_str || !node_id_str) {
          printf("Invalid arguments\n");
          break;
        }
        uint64_t node_id = strtoull(node_id_str, NULL, 16);
        BmConfigPartition partition;
        if (strncmp("u", part_str, part_str_len) == 0) {
          partition = BM_CFG_PARTITION_USER;
        } else if (strncmp("s", part_str, part_str_len) == 0) {
          partition = BM_CFG_PARTITION_SYSTEM;
        } else {
          printf("Invalid arguments\n");
          break;
        }
        BmErr err;
        if (!bcmp_config_status_request(node_id, partition, &err, NULL)) {
          printf("Failed to send status request \n");
        } else {
          printf("Successful status request send\n");
        }
      } else if (strncmp("del", cmd_id_str, cmd_id_str_len) == 0) {
        const char *node_id_str;
        BaseType_t node_id_str_len = 0;
        node_id_str = FreeRTOS_CLIGetParameter(commandString, 3, &node_id_str_len);
        const char *part_str;
        BaseType_t part_str_len = 0;
        part_str = FreeRTOS_CLIGetParameter(commandString, 4, &part_str_len);
        const char *key_str;
        BaseType_t key_str_str_len = 0;
        key_str = FreeRTOS_CLIGetParameter(commandString, 5, &key_str_str_len);
        if (!key_str || !part_str || !node_id_str) {
          printf("Invalid arguments\n");
          break;
        }
        uint64_t node_id = strtoull(node_id_str, NULL, 16);
        BmConfigPartition partition;
        if (strncmp("u", part_str, part_str_len) == 0) {
          partition = BM_CFG_PARTITION_USER;
        } else if (strncmp("s", part_str, part_str_len) == 0) {
          partition = BM_CFG_PARTITION_SYSTEM;
        } else {
          printf("Invalid arguments\n");
          break;
        }
        if (bcmp_config_del_key(node_id, partition, key_str_str_len, key_str, NULL)) {
          printf("Successfully sent del key request\n");
        } else {
          printf("Failed to send del key request\n");
        }
      } else {
        printf("Invalid arguments\n");
        break;
      }
    } else if (strncmp("time", command, command_str_len) == 0) {
      const char *cmdstr;
      BaseType_t cmdstr_len = 0;
      cmdstr = FreeRTOS_CLIGetParameter(commandString, 2, &cmdstr_len);
      const char *node_id_str;
      BaseType_t node_id_str_len = 0;
      node_id_str = FreeRTOS_CLIGetParameter(commandString, 3, &node_id_str_len);
      if (!cmdstr || !node_id_str) {
        printf("Invalid arguments\n");
        break;
      }
      uint64_t node_id = strtoull(node_id_str, NULL, 16);
      if (strncmp("set", cmdstr, cmdstr_len) == 0) {
        const char *utc_us_str;
        BaseType_t utc_us_str_len = 0;
        utc_us_str = FreeRTOS_CLIGetParameter(commandString, 4, &utc_us_str_len);
        if (!utc_us_str) {
          printf("Invaid params\n");
          break;
        }
        uint64_t utc_us = strtoull(utc_us_str, NULL, 0);
        if (bcmp_time_set_time(node_id, utc_us) != BmOK) {
          printf("bcmp set time failed to be sent\n");
          break;
        } else {
          printf("succesfully sent time set cmd\n");
        }
      } else if (strncmp("get", cmdstr, cmdstr_len) == 0) {
        if (bcmp_time_get_time(node_id) != BmOK) {
          printf("bcmp get time failed to be sent\n");
          break;
        } else {
          printf("succesfully sent time get cmd\n");
        }
      } else {
        printf("Invalid arguments\n");
        break;
      }
    } else if (strncmp("topo", command, command_str_len) == 0) {
      bcmp_topology_start(network_topology_print); // use generic print as callback
    } else if (strncmp("resources", command, command_str_len) == 0) {
      const char *node_id_str;
      BaseType_t node_id_str_len = 0;
      node_id_str = FreeRTOS_CLIGetParameter(commandString, 2, &node_id_str_len);
      if (!node_id_str) {
        bcmp_resource_discovery_print_resources();
        break;
      }
      uint64_t node_id = strtoull(node_id_str, NULL, 16);
      if (bcmp_resource_discovery_send_request(node_id, NULL) != BmOK) {
        printf("Failed to send discovery request.\n");
      } else {
        printf("Sent discovery request to %016" PRIx64 "\n", node_id);
      }
    } else if (strncmp("sub", command, command_str_len) == 0) {
      const char *topicStr = FreeRTOS_CLIGetParameter(commandString, 2, &command_str_len);

      if (command_str_len == 0) {
        printf("ERR topic required\n");
        break;
      }

      char *topic = static_cast<char *>(pvPortMalloc(command_str_len + 1));
      configASSERT(topic);
      memcpy(topic, topicStr, command_str_len);
      topic[command_str_len] = 0;

      bm_sub(topic, print_subscriptions);
      vPortFree(topic);

    } else if (strncmp("unsub", command, command_str_len) == 0) {
      const char *topicStr = FreeRTOS_CLIGetParameter(commandString, 2, &command_str_len);

      if (command_str_len == 0) {
        printf("ERR topic required\n");
        break;
      }

      char *topic = static_cast<char *>(pvPortMalloc(command_str_len + 1));
      configASSERT(topic);
      memcpy(topic, topicStr, command_str_len);
      topic[command_str_len] = 0;

      bm_unsub(topic, print_subscriptions);
      vPortFree(topic);

    } else if (strncmp("pub", command, command_str_len) == 0) {
      const char *topicStr = FreeRTOS_CLIGetParameter(commandString, 2, &command_str_len);

      if (command_str_len == 0) {
        printf("ERR topic required\n");
        break;
      }

      char *topic = static_cast<char *>(pvPortMalloc(command_str_len + 1));
      configASSERT(topic);
      memcpy(topic, topicStr, command_str_len);
      topic[command_str_len] = 0;
      BaseType_t dataStrLen;

      const char *dataStr = FreeRTOS_CLIGetParameter(commandString, 3, &dataStrLen);

      if (dataStrLen == 0) {
        printf("ERR data required\n");
        break;
      }
      const char *typeStr = FreeRTOS_CLIGetParameter(commandString, 4, &command_str_len);

      if (command_str_len == 0) {
        printf("ERR data required\n");
        break;
      }
      const char *versionStr = FreeRTOS_CLIGetParameter(commandString, 5, &command_str_len);
      if (command_str_len == 0) {
        printf("ERR data required\n");
        break;
      }
      uint8_t type = strtoul(typeStr, NULL, 0);
      uint8_t version = strtoul(versionStr, NULL, 0);
      uint8_t *data = static_cast<uint8_t *>(pvPortMalloc(dataStrLen + 1));
      configASSERT(data);
      memcpy(data, dataStr, dataStrLen);
      data[dataStrLen] = 0;

      if (bm_pub(topic, data, dataStrLen, type, version) == BmOK) {
        printf("Successfully published message to topic: %s of %d bytes\n", topic, dataStrLen);
      } else {
        printf("Could not publish message to topic: %s\n", topic);
      }
      vPortFree(topic);
      vPortFree(data);
    } else {
      printf("ERR Invalid arguments\n");
    }
  } while (0);

  return pdFALSE;
}

void bcmp_cli_init() { FreeRTOS_CLIRegisterCommand(&cmd_bcmp); }
