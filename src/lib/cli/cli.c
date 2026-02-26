#include <stdbool.h>
#include <string.h>

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "FreeRTOS_CLI.h"
#include "queue.h"
#include "task.h"

#include "cli.h"
#include "debug.h"
// #include "log.h"
#include "task_priorities.h"
#include "tokenize.h"

// Number of commands to queue up before dropping them
#define CMD_QUEUE_LEN 32

// Maximum number of characters in a single command output line
#define CMD_OUTPUT_BUFF_LEN 256

static void cliTask( void *parameters );
extern xQueueHandle debugPrintQueue;

xQueueHandle cliQueue = NULL;

xQueueHandle cliGetQueue() {
  return cliQueue;
}

void startCLI() {
  BaseType_t rval;

  /* Create the queue used to pass messages from the queue send task to the
  queue receive task. */
  cliQueue = xQueueCreate(CMD_QUEUE_LEN, sizeof(CLICommand_t));
  configASSERT(cliQueue != 0);

  rval = xTaskCreate(
              cliTask,
              "CLI",
              // TODO - verify stack size
              configMINIMAL_STACK_SIZE * 16,
              NULL,
              CLI_TASK_PRIORITY,
              NULL);

  configASSERT(rval == pdTRUE);
}

static void cliTask( void *parameters ) {
  // Don't warn about unused parameters
  (void) parameters;

  for(;;) {
    CLICommand_t command;

    BaseType_t rval = xQueueReceive(cliQueue, &command, portMAX_DELAY);
    configASSERT(rval == pdTRUE);
    configASSERT(command.buff != NULL);

    FreeRTOS_CLIProcessCommand((char *)command.buff, NULL, 0);

    // Make sure we free the buffer
    vPortFree(command.buff);
  }
}

/*!
  Process received CLI commands from iridium. Split them up, if multiple,
  and send to CLI processor.

  \param[in] *response - Iridium command response which contains CLI command(s)
  \param[in] len - Response length (including required terminating \0)
  \return true if caller must free response, false if it has already been freed
*/
bool _processRemoteCLICommands(uint8_t *response, size_t len) {
  size_t numTokens = 0;
  char **tokens = NULL;
  bool rval = true; // Caller should free response buffer

  tokens = tokenize((char *)response, len, '\n', &numTokens);
  if (tokens != NULL) {
    for(uint32_t token = 0; token < numTokens; token++) {
      size_t cmdLen = strnlen(tokens[token], len) + 1;
      // Set up command buffer structure
      CLICommand_t cliCmd = {
        .buff = (uint8_t *)pvPortMalloc(cmdLen),
        .len = (uint16_t)(cmdLen),
        .src = NULL
      };

      configASSERT(cliCmd.buff != NULL);
      memcpy(cliCmd.buff, tokens[token], cmdLen);

      // Send line to CLI processor
      if(xQueueSend(cliGetQueue(), &cliCmd, 10 ) == errQUEUE_FULL) {
        // CLI queue was full, lets reuse the allocated buffer
        // logPrint(SYSLog, LOG_LEVEL_DEBUG, "Error sending command to CLI queue\n");
        vPortFree(cliCmd.buff);
      }
    }
    vPortFree(tokens);
  } else {
    // Set up command buffer structure
    CLICommand_t cliCmd = {
      .buff = response,
      .len = (uint16_t)len,
      .src = NULL
    };

    // Send line to CLI processor
    if(xQueueSend(cliGetQueue(), &cliCmd, 10 ) == errQUEUE_FULL) {
      // CLI queue was full, lets reuse the allocated buffer
      // logPrint(SYSLog, LOG_LEVEL_DEBUG, "Error sending command to CLI queue\n");
    } else {
      // Let caller know not to free response buffer, since we
      // sent it as the CLI command buff
      rval = false;
    }
  }

  return rval;
}

void cliIridiumRxCallback(uint8_t *message, size_t len) {
    uint8_t *response = (uint8_t *)pvPortMalloc(len + 1);
    configASSERT(response != NULL);

    // Copy response to buffer and null terminating string
    memcpy(response, message, len);
    response[len] = 0;

    // logPrint(SYSLog, LOG_LEVEL_INFO, "Remote message received(%u)! \"%s\"\n", len, response);

    if(_processRemoteCLICommands(response, len + 1)) {
      vPortFree(response);
    }
}
