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
  "bcmp ping <node_id>\n",
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
        uint64_t node_id = strtoull(node_id_str, NULL, 0);
        uint8_t payload[32] = {0};
        if(bcmp_send_ping_request(node_id, &multicast_global_addr, payload, 32) != ERR_OK) {
          printf("Error sending ping\n");
        }
      } else {
        printf("Invalid node_id\n");
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
