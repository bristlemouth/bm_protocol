#include <string.h>
#include "FreeRTOS.h"
#include "task.h"

// FreeRTOS+CLI includes.
#include "FreeRTOS_CLI.h"

#include "bcmp.h"
#include "bcmp_cli.h"

#include "bcmp_heartbeat.h"
#include "bcmp_neighbors.h"
#include "bcmp_info.h"
#include "bcmp_ping.h"
#include "bcmp_config.h"
#include "util.h"
#include "bcmp_time.h"
#include "bcmp_topology.h"
#include "bcmp_resource_discovery.h"

#include "debug.h"

static BaseType_t cmd_bcmp_fn( char *writeBuffer,
                                  size_t writeBufferLen,
                                  const char *commandString);
static const CLI_Command_Definition_t cmd_bcmp = {
  // Command string
  "bcmp",
  // Help string
  "bcmp neighbors\n"
  "bcmp info <node_id>\n"
  "bcmp ping <node_id>\n"
  "bcmp cfg get <node_id> <partition(u/s)> <key>\n"
  "bcmp cfg set <node_id> <partition(u/s)> <type(u/i/f/s/b)> <key> <value>\n"
  "bcmp cfg commit <node_id> <partition(u/s)>\n"
  "bcmp cfg status <node_id> <partition(u/s)>\n"
  "bcmp cfg del <node_id> <partition(u/s)> <key>\n"
  "bcmp time set <node_id> <utc_us>\n"
  "bcmp time get <node_id>\n"
  "bcmp topo\n"
  "bcmp resources\n"
  "bcmp resources <node_id>\n",
  // Command function
  cmd_bcmp_fn,
  // Number of parameters
  -1
};

void print_neighbor_basic(bm_neighbor_t *neighbor) {
  printf("%" PRIx64 " |   %u  | %7s | %0.3f\n",
          neighbor->node_id,
          neighbor->port,
          neighbor->online ? "online": "offline",
          (float)((xTaskGetTickCount() - neighbor->last_heartbeat_ticks))/1000.0);
}

static BaseType_t cmd_bcmp_fn(char *writeBuffer,
                            size_t writeBufferLen,
                            const char *commandString) {
  (void) writeBuffer;
  (void) writeBufferLen;
  (void) commandString;

  do {
    const char *command;
    BaseType_t command_str_len;
    command = FreeRTOS_CLIGetParameter(
                    commandString,
                    1,
                    &command_str_len);

    if(strncmp("neighbors", command, command_str_len) == 0) {
      printf("    Node ID      | Port |  State  | Time since last heartbeat (s)\n");
      bcmp_neighbor_foreach(print_neighbor_basic);
    } else if(strncmp("info", command, command_str_len) == 0) {
      const char *node_id_str;
      BaseType_t node_id_str_len = 0;
      node_id_str = FreeRTOS_CLIGetParameter(
                      commandString,
                      2,
                      &node_id_str_len);

      if(node_id_str_len > 0) {
        uint64_t node_id = strtoull(node_id_str, NULL, 16);

        bm_neighbor_t *neighbor = bcmp_find_neighbor(node_id);
        if(neighbor) {
          bcmp_print_neighbor_info(neighbor);
        } else {
          printf("Device not in neighbor table. Sending global request.\n");
          bcmp_expect_info_from_node_id(node_id);
          if(bcmp_request_info(node_id, &multicast_global_addr) != ERR_OK) {
            printf("Error sending request\n");
          }
        }
      } else {
        printf("Invalid arguments\n");
      }
    } else if (strncmp("ping", command, command_str_len) == 0){
      const char *node_id_str;
      BaseType_t node_id_str_len = 0;
      node_id_str = FreeRTOS_CLIGetParameter(
                      commandString,
                      2,
                      &node_id_str_len);
      if(node_id_str_len > 0) {
        uint64_t node_id = strtoull(node_id_str, NULL, 16);
        uint8_t payload[32] = { 0 };
        if(bcmp_send_ping_request(node_id, &multicast_global_addr, payload, 32) != ERR_OK) {
          printf("Error sending ping\n");
        }
      } else {
        printf("Invalid node_id\n");
      }
    } else if(strncmp("cfg", command, command_str_len) == 0) {
      const char *cmd_id_str;
      BaseType_t cmd_id_str_len = 0;
      cmd_id_str = FreeRTOS_CLIGetParameter(
                      commandString,
                      2,
                      &cmd_id_str_len);
      if(!cmd_id_str) {
        printf("Invalid arguments\n");
        break;
      }
      if(strncmp("get", cmd_id_str, cmd_id_str_len) == 0) {
        const char *node_id_str;
        BaseType_t node_id_str_len = 0;
        node_id_str = FreeRTOS_CLIGetParameter(
                        commandString,
                        3,
                        &node_id_str_len);
        const char *part_str;
        BaseType_t part_str_len = 0;
        part_str = FreeRTOS_CLIGetParameter(
                        commandString,
                        4,
                        &part_str_len);
        const char *key_str;
        BaseType_t key_str_str_len = 0;
        key_str = FreeRTOS_CLIGetParameter(
                        commandString,
                        5,
                        &key_str_str_len);
        if(!key_str || !part_str || !node_id_str){
          printf("Invalid arguments\n");
          break;
        }
        uint64_t node_id = strtoull(node_id_str, NULL, 0);
        bm_common_config_partition_e partition;
        if (strncmp("u", part_str, part_str_len) == 0) {
          partition = BM_COMMON_CFG_PARTITION_USER;
        } else if (strncmp("s", part_str, part_str_len) == 0) {
          partition = BM_COMMON_CFG_PARTITION_SYSTEM;
        } else {
          printf("Invalid arguments\n");
          break;
        }
        err_t err;
        if(!bcmp_config_get(node_id,partition,key_str_str_len,key_str,err)){
          printf("Failed to send message config get\n");
        } else {
          printf("Succesfully sent config get msg\n");
        }
      } else if (strncmp("set", cmd_id_str, cmd_id_str_len) == 0) {
        const char *node_id_str;
        BaseType_t node_id_str_len = 0;
        node_id_str = FreeRTOS_CLIGetParameter(
                        commandString,
                        3,
                        &node_id_str_len);
        const char *part_str;
        BaseType_t part_str_len = 0;
        part_str = FreeRTOS_CLIGetParameter(
                        commandString,
                        4,
                        &part_str_len);
        const char *type_str;
        BaseType_t type_str_str_len = 0;
        type_str = FreeRTOS_CLIGetParameter(
                        commandString,
                        5,
                        &type_str_str_len);
        const char *key_str;
        BaseType_t key_str_str_len = 0;
        key_str = FreeRTOS_CLIGetParameter(
                        commandString,
                        6,
                        &key_str_str_len);
        const char *value_str;
        BaseType_t value_str_str_len = 0;
        value_str = FreeRTOS_CLIGetParameter(
                        commandString,
                        7,
                        &value_str_str_len);
        if(!key_str || !part_str || !type_str || !node_id_str || !value_str){
          printf("Invalid arguments\n");
          break;
        }
        uint64_t node_id = strtoull(node_id_str, NULL, 0);
        bm_common_config_partition_e partition;
        if (strncmp("u", part_str, part_str_len) == 0) {
          partition = BM_COMMON_CFG_PARTITION_USER;
        } else if (strncmp("s", part_str, part_str_len) == 0) {
          partition = BM_COMMON_CFG_PARTITION_SYSTEM;
        } else {
          printf("Invalid arguments\n");
          break;
        }
        cfg::ConfigDataTypes_e type;
        if (strncmp("u", type_str, type_str_str_len) == 0){
          type = cfg::ConfigDataTypes_e::UINT32;
        } else if (strncmp("i", type_str, type_str_str_len) == 0) {
          type = cfg::ConfigDataTypes_e::INT32;
        } else if (strncmp("f", type_str, type_str_str_len) == 0) {
          type = cfg::ConfigDataTypes_e::FLOAT;
        } else if (strncmp("s", type_str, type_str_str_len) == 0) {
          type = cfg::ConfigDataTypes_e::STR;
        } else if (strncmp("b", type_str, type_str_str_len) == 0) {
          type = cfg::ConfigDataTypes_e::BYTES;
        } else {
          printf("Invalid arguments\n");
          break;
        }
        err_t err;
        size_t buffer_size = cfg::MAX_STR_LEN_BYTES;
        uint8_t* cbor_buf = static_cast<uint8_t *>(pvPortMalloc(buffer_size));
        configASSERT(cbor_buf);
        memset(cbor_buf, 0, cfg::MAX_STR_LEN_BYTES);
        CborEncoder encoder;
        cbor_encoder_init(&encoder, cbor_buf, buffer_size, 0);
        switch(type){
          case cfg::ConfigDataTypes_e::UINT32: {
            uint32_t val = strtoul(value_str,NULL,0);
            if(cbor_encode_uint(&encoder, val)!= CborNoError) {
                break;
            }
            if(!bcmp_config_set(node_id,partition,key_str_str_len,key_str,buffer_size,cbor_buf,err)){
              printf("Failed to send message config set\n");
            } else {
              printf("Succesfully sent config set msg\n");
            }
            break;
          }
          case cfg::ConfigDataTypes_e::INT32: {
            int32_t val = strtol(value_str,NULL,0);
            if(cbor_encode_int(&encoder, val)!= CborNoError) {
                break;
            }
            if(!bcmp_config_set(node_id,partition,key_str_str_len,key_str,buffer_size,cbor_buf,err)){
              printf("Failed to send message config get\n");
            } else {
              printf("Succesfully sent config get msg\n");
            }
            break;
          }
          case cfg::ConfigDataTypes_e::FLOAT: {
            float val;
            if(!bStrtof(const_cast<char *>(value_str), &val)){
              printf("Invalid param\n");
              break;
            }
            if(cbor_encode_float(&encoder, val)!= CborNoError) {
                break;
            }
            if(!bcmp_config_set(node_id,partition,key_str_str_len,key_str,buffer_size,cbor_buf,err)){
              printf("Failed to send message config get\n");
            } else {
              printf("Succesfully sent config get msg\n");
            }
            break;
          }
          case cfg::ConfigDataTypes_e::STR: {
            if(cbor_encode_text_stringz(&encoder, value_str)!= CborNoError) {
                break;
            }
            if(!bcmp_config_set(node_id,partition,key_str_str_len,key_str,buffer_size,cbor_buf,err)){
              printf("Failed to send message config get\n");
            } else {
              printf("Succesfully sent config get msg\n");
            }
            break;
          }
          case cfg::ConfigDataTypes_e::BYTES: {
            if(cbor_encode_byte_string(&encoder, reinterpret_cast<const uint8_t*>(value_str), strlen(value_str))!= CborNoError) {
                break;
            }
            if(!bcmp_config_set(node_id,partition,key_str_str_len,key_str,buffer_size,cbor_buf,err)){
              printf("Failed to send message config get\n");
            } else {
              printf("Succesfully sent config get msg\n");
            }
            break;
          }
          default:
            break;
        }
        vPortFree(cbor_buf);
      } else if (strncmp("commit", cmd_id_str, cmd_id_str_len) == 0) {
        const char *node_id_str;
        BaseType_t node_id_str_len = 0;
        node_id_str = FreeRTOS_CLIGetParameter(
                        commandString,
                        3,
                        &node_id_str_len);
        const char *part_str;
        BaseType_t part_str_len = 0;
        part_str = FreeRTOS_CLIGetParameter(
                        commandString,
                        4,
                        &part_str_len);
        if(!part_str || !node_id_str){
          printf("Invalid arguments\n");
          break;
        }
        uint64_t node_id = strtoull(node_id_str, NULL, 0);
        bm_common_config_partition_e partition;
        if (strncmp("u", part_str, part_str_len) == 0) {
          partition = BM_COMMON_CFG_PARTITION_USER;
        } else if (strncmp("s", part_str, part_str_len) == 0) {
          partition = BM_COMMON_CFG_PARTITION_SYSTEM;
        } else {
          printf("Invalid arguments\n");
          break;
        }
        err_t err;
        if(!bcmp_config_commit(node_id,partition,err)){
          printf("Failed to send config commit\n");
        } else {
          printf("Succesfull config commit send\n");
        }
      } else if (strncmp("status", cmd_id_str, cmd_id_str_len) == 0) {
        const char *node_id_str;
        BaseType_t node_id_str_len = 0;
        node_id_str = FreeRTOS_CLIGetParameter(
                        commandString,
                        3,
                        &node_id_str_len);
        const char *part_str;
        BaseType_t part_str_len = 0;
        part_str = FreeRTOS_CLIGetParameter(
                        commandString,
                        4,
                        &part_str_len);
        if(!part_str || !node_id_str){
          printf("Invalid arguments\n");
          break;
        }
        uint64_t node_id = strtoull(node_id_str, NULL, 0);
        bm_common_config_partition_e partition;
        if (strncmp("u", part_str, part_str_len) == 0) {
          partition = BM_COMMON_CFG_PARTITION_USER;
        } else if (strncmp("s", part_str, part_str_len) == 0) {
          partition = BM_COMMON_CFG_PARTITION_SYSTEM;
        } else {
          printf("Invalid arguments\n");
          break;
        }
        err_t err;
        if(!bcmp_config_status_request(node_id,partition,err)){
          printf("Failed to send status request \n");
        } else {
          printf("Succesfull status request send\n");
        }
      } else if (strncmp("del", cmd_id_str, cmd_id_str_len) == 0){
        const char *node_id_str;
        BaseType_t node_id_str_len = 0;
        node_id_str = FreeRTOS_CLIGetParameter(
                        commandString,
                        3,
                        &node_id_str_len);
        const char *part_str;
        BaseType_t part_str_len = 0;
        part_str = FreeRTOS_CLIGetParameter(
                        commandString,
                        4,
                        &part_str_len);
        const char *key_str;
        BaseType_t key_str_str_len = 0;
        key_str = FreeRTOS_CLIGetParameter(
                        commandString,
                        5,
                        &key_str_str_len);
        if(!key_str || !part_str || !node_id_str){
          printf("Invalid arguments\n");
          break;
        }
        uint64_t node_id = strtoull(node_id_str, NULL, 0);
        bm_common_config_partition_e partition;
        if (strncmp("u", part_str, part_str_len) == 0) {
          partition = BM_COMMON_CFG_PARTITION_USER;
        } else if (strncmp("s", part_str, part_str_len) == 0) {
          partition = BM_COMMON_CFG_PARTITION_SYSTEM;
        } else {
          printf("Invalid arguments\n");
          break;
        }
        if(bcmp_config_del_key(node_id, partition, key_str_str_len, key_str)){
          printf("Successfully sent del key request\n");
        } else {
          printf("Failed to send del key request\n");
        }
      } else {
        printf("Invalid arguments\n");
        break;
      }
    }
    else if(strncmp("time", command, command_str_len) == 0) {
      const char *cmdstr;
      BaseType_t cmdstr_len = 0;
      cmdstr = FreeRTOS_CLIGetParameter(
                      commandString,
                      2,
                      &cmdstr_len);
      const char *node_id_str;
      BaseType_t node_id_str_len = 0;
      node_id_str = FreeRTOS_CLIGetParameter(
                      commandString,
                      3,
                      &node_id_str_len);
      if (!cmdstr || !node_id_str) {
        printf("Invalid arguments\n");
        break;
      }
      uint64_t node_id = strtoull(node_id_str, NULL, 0);
      if (strncmp("set", cmdstr, cmdstr_len) == 0) {
        const char *utc_us_str;
        BaseType_t utc_us_str_len = 0;
        utc_us_str = FreeRTOS_CLIGetParameter(
                        commandString,
                        4,
                        &utc_us_str_len);
        if(!utc_us_str) {
          printf("Invaid params\n");
          break;
        }
        uint64_t utc_us = strtoull(utc_us_str, NULL, 0);
        if(!bcmp_time_set_time(node_id, utc_us)) {
          printf("bcmp set time failed to be sent\n");
          break;
        } else {
          printf("succesfully sent time set cmd\n");
        }
      } else if (strncmp("get", cmdstr, cmdstr_len) == 0) {
        if(!bcmp_time_get_time(node_id)){
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
      bcmp_topology_start(networkTopologyPrint); // use generic print as callback
    } else if (strncmp("resources", command, command_str_len) == 0) {
      const char *node_id_str;
      BaseType_t node_id_str_len = 0;
      node_id_str = FreeRTOS_CLIGetParameter(
                      commandString,
                      2,
                      &node_id_str_len);
      if (!node_id_str) {
        bcmp_resource_discovery::bcmp_resource_discovery_print_resources();
        break;
      }
      uint64_t node_id = strtoull(node_id_str, NULL, 0);
      if(!bcmp_resource_discovery::bcmp_resource_discovery_send_request(node_id)){
        printf("Failed to send discovery request.\n");
      } else {
        printf("Sent discovery request to %" PRIx64 "\n", node_id);
      }
    } else {
      printf("Invalid arguments\n");
    }
  } while(0);


  return pdFALSE;
}

void bcmp_cli_init() {
  FreeRTOS_CLIRegisterCommand( &cmd_bcmp );
}
