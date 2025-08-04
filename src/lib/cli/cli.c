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

