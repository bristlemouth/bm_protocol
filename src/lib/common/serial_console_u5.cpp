#include <stdbool.h>
#include "cli.h"
#include "debug.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "serial.h"
#include "serial_console.h"
#include "stream_buffer.h"
#include "task.h"
#include "task_priorities.h"
#include "app_util.h"

// Enable echo
const bool bEcho = true;

static void processConsoleRxByte(void *serialHandle, uint8_t byte);

static SerialMessage_t xConsoleOutputMessage = {NULL, 0, NULL};

static uint8_t *ulConsoleRxBuff;
static uint32_t ulConsoleBuffIdx;

// Default print to usb
static SerialHandle_t *serialConsoleHandle;

void startSerialConsole(SerialHandle_t *handle) {
  BaseType_t rval;
  configASSERT(handle);

  serialConsoleHandle = handle;
  serialConsoleHandle->processByte = processConsoleRxByte;

  // Single byte trigger level for fast response time
  serialConsoleHandle->txStreamBuffer = xStreamBufferCreate(serialConsoleHandle->txBufferSize, 1);
  configASSERT(serialConsoleHandle->txStreamBuffer != NULL);

  serialConsoleHandle->rxStreamBuffer = xStreamBufferCreate(serialConsoleHandle->rxBufferSize, 1);
  configASSERT(serialConsoleHandle->rxStreamBuffer != NULL);

  // Buffer to hold data before sending to CLI
  ulConsoleRxBuff = static_cast<uint8_t *>(pvPortMalloc(CONSOLE_RX_BUFF_SIZE));
  configASSERT(ulConsoleRxBuff != NULL);

  xConsoleOutputMessage.buff = static_cast<uint8_t *>(pvPortMalloc(CONSOLE_OUTPUT_SIZE));
  configASSERT(xConsoleOutputMessage.buff != NULL);
  xConsoleOutputMessage.len = 0;

  debugInit(serialConsolePutchar);

  rval = xTaskCreate(
              serialGenericRxTask,
              "ConsoleRX",
              configMINIMAL_STACK_SIZE * 2,
              serialConsoleHandle,
              CONSOLE_RX_TASK_PRIORITY,
              NULL);
  configASSERT(rval == pdTRUE);

}

void serialConsolePutcharUnbuffered(const char chr) {
  if(serialConsoleHandle){
   serialPutcharUnbuffered(serialConsoleHandle, chr);
  }
}

void _putchar(char character) {
  if (serialConsoleHandle){
    serialPutcharUnbuffered(serialConsoleHandle, character);
  }
}

void serialConsolePutchar(char character, void* arg) {
  (void) arg;
  // Only print if the interface is connected
  if(serialConsoleHandle->enabled) {
    xConsoleOutputMessage.buff[xConsoleOutputMessage.len++] = (uint8_t)character;

    // If message buffer is full (or we see a newline), send buffer
    if (xConsoleOutputMessage.len >= CONSOLE_OUTPUT_SIZE || character == '\n') {
      xConsoleOutputMessage.destination = serialConsoleHandle;
      if(xQueueSend(serialGetTxQueue(), &xConsoleOutputMessage, 10) != pdTRUE) {
        // Free buffer if unable to send
        vPortFree(xConsoleOutputMessage.buff);
      }

      // Allocate console buffer
      xConsoleOutputMessage.buff = static_cast<uint8_t *>(pvPortMalloc(CONSOLE_OUTPUT_SIZE));
      configASSERT(xConsoleOutputMessage.buff != NULL);
      xConsoleOutputMessage.len = 0;
    }
  }
  // TODO - do other things with the data if interface is not enabled
  // perhaps save to log file
}

static void processConsoleRxByte(void *serialHandle, uint8_t byte) {
  configASSERT(serialHandle != NULL);
  SerialHandle_t *handle = static_cast<SerialHandle_t *>(serialHandle);

  ulConsoleRxBuff[ulConsoleBuffIdx] = byte;

  if(bEcho) {
    serialConsolePutcharUnbuffered((char)ulConsoleRxBuff[ulConsoleBuffIdx]);

    // Erase last character with backspace
    if((char)ulConsoleRxBuff[ulConsoleBuffIdx] == '\b') {
      serialConsolePutcharUnbuffered(' ');
      serialConsolePutcharUnbuffered('\b');
    }
  }

  if(ulConsoleRxBuff[ulConsoleBuffIdx] == '\n'){

    // Deal with the carriage return, if any
    if(ulConsoleBuffIdx > 0 && ulConsoleRxBuff[ulConsoleBuffIdx - 1] == '\r') {
      ulConsoleRxBuff[ulConsoleBuffIdx - 1] = 0;
    } else {
      ulConsoleRxBuff[ulConsoleBuffIdx] = 0;
    }

    // Set up command buffer structure
    CLICommand_t cliCmd = {
      .buff = ulConsoleRxBuff,
      .len = (uint16_t)ulConsoleBuffIdx,
      .src = handle
    };

    // Send line to CLI processor
    BaseType_t rval = xQueueSend(cliGetQueue(), &cliCmd, 0U );
    if(rval == errQUEUE_FULL) {
      // CLI queue was full, lets reuse the allocated buffer
      // Could print out an error message instead?
    } else {
      // Buffer was sent, it is up to the CLI task to free
      // Allocate next rx buffer
      ulConsoleRxBuff = static_cast<uint8_t *>(pvPortMalloc(CONSOLE_RX_BUFF_SIZE));
      configASSERT(ulConsoleRxBuff != NULL);
    }

    // Reset buffer index
    ulConsoleBuffIdx = 0;
  } else {
    // Handle backspaces
    if((char)ulConsoleRxBuff[ulConsoleBuffIdx] == '\b') {
      if(ulConsoleBuffIdx > 0) {
        ulConsoleBuffIdx--;
      } // ignore backspace otherwise
    } else {
      ulConsoleBuffIdx++;
    }

    // Not enough room in input buffer. Purge data
    if(ulConsoleBuffIdx >= CONSOLE_RX_BUFF_SIZE) {
      printf("\nRX overflow - clearing buffer!\n");
      ulConsoleBuffIdx = 0;
    }

  }
}

/*!
  Enable serial device for serialConsoleHandle and reset all buffers. Also,
  call usbConnected() in case we have to do anything when console
  is opened.
*/
void serialConsoleEnable() {
  serialConsoleHandle->enabled = true;
  ulConsoleBuffIdx = 0;
  serialConsoleHandle->flags &= ~SERIAL_TX_IN_PROGRESS;
  xStreamBufferReset(serialConsoleHandle->rxStreamBuffer);
  xStreamBufferReset(serialConsoleHandle->txStreamBuffer);
}

/*!
  Disable serialConsoleHandle serial device
*/
void serialConsoleDisable() {
  serialConsoleHandle->enabled = false;
}
