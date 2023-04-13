#include "FreeRTOS.h"
#include "task.h"

// FreeRTOS+CLI includes.
#include "FreeRTOS_CLI.h"

#include "bcmp.h"
#include "bcmp_cli.h"
#include "debug.h"

static BaseType_t cmd_ping_fn( char *writeBuffer,
                                  size_t writeBufferLen,
                                  const char *commandString);

static const CLI_Command_Definition_t cmd_ping = {
  // Command string
  "ping",
  // Help string
  "ping <addr>\n",
  // Command function
  cmd_ping_fn,
  // Number of parameters
  0
};

static uint32_t count;
static BaseType_t cmd_ping_fn(char *writeBuffer,
                            size_t writeBufferLen,
                            const char *commandString) {
  (void) writeBuffer;
  (void) writeBufferLen;
  (void)commandString;

  do {
    // const char *ip_addr_str;
    // BaseType_t ip_addr_str_len;
    // ip_addr_str = FreeRTOS_CLIGetParameter(
    //                 commandString,
    //                 1, // Get the first ip_addr_str (command)
    //                 &ip_addr_str_len);

    // if(ip_addr_str == NULL) {
    //   printf("ERR Invalid paramters\n");
    //   break;
    // }

    // ip_addr_t addr;
    // if(ip6addr_aton(ip_addr_str, &addr) == 0) {
    //   printf("Invalid IP address\n");
    //   break;
    // }

    // bcmp_send_ping(&addr);
    bcmp_send_heartbeat(count++);
  } while(0);


  return pdFALSE;
}

void bcmp_cli_init() {
  FreeRTOS_CLIRegisterCommand( &cmd_ping );
}
