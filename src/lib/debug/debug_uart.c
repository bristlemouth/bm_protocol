
/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "stream_buffer.h"
#include "queue.h"

#include "FreeRTOS_CLI.h"

#include "debug_uart.h"
#include "cli.h"
#include <stdlib.h>
#include <string.h>
// #include "usbd_cdc_if.h"

#include "serial.h"
#include "serial_console.h"
#include "debug.h"
#include "bsp.h"
#include "task_priorities.h"
// #include "log.h"

static void processLineBufferedRxByte(void *serialHandle, uint8_t byte);
static void printLine(void *serialHandle, uint8_t *line, size_t len);

// Log_t *UARTLog;

#ifdef DEBUG_USE_LPUART1
#define LPUART1_LINE_BUFF_LEN 64
static uint8_t lpUart1Buffer[LPUART1_LINE_BUFF_LEN];
SerialLineBuffer_t lpUART1LineBuffer = {
  lpUart1Buffer,
  0,
  sizeof(lpUart1Buffer),
  printLine
};
SerialHandle_t lpuart1  = {
  .device = LPUART1,
  .name = "lpuart",
  .txPin = NULL,
  .rxPin = NULL,
  .interruptPin = NULL,
  .txStreamBuffer = NULL,
  .rxStreamBuffer = NULL,
  .txBufferSize = 128,
  .rxBufferSize = 64,
  .rxBytesFromISR = serialGenericRxBytesFromISR,
  .getTxBytesFromISR = serialGenericGetTxBytesFromISR,
  .processByte = processLineBufferedRxByte,
  .data = &lpUART1LineBuffer,
  .enabled = false,
  .flags = 0,
  .preTxCb = NULL,
  .postTxCb = NULL,
};
#endif

#ifdef DEBUG_USE_USART1
#define USART1_LINE_BUFF_LEN 64
static uint8_t usart1Buffer[USART1_LINE_BUFF_LEN];
SerialLineBuffer_t usart1LineBuffer = {
  usart1Buffer,
  0,
  sizeof(usart1Buffer),
  printLine
};
SerialHandle_t usart1   = {
  .device = USART1,
  .name = "usart1",
  .txPin = NULL,
  .rxPin = NULL,
  .interruptPin = NULL,
  .txStreamBuffer = NULL,
  .rxStreamBuffer = NULL,
  .txBufferSize = 64,
  .rxBufferSize = 128,
  .rxBytesFromISR = serialGenericRxBytesFromISR,
  .getTxBytesFromISR = serialGenericGetTxBytesFromISR,
  .processByte = processLineBufferedRxByte,
  .data = &usart1LineBuffer,
  .enabled = false,
  .flags = 0,
  .preTxCb = NULL,
  .postTxCb = NULL,
};
#endif

#ifdef DEBUG_USE_USART2
#define USART2_LINE_BUFF_LEN 84
static uint8_t usart2Buffer[USART2_LINE_BUFF_LEN];
SerialLineBuffer_t usart2LineBuffer = {
  usart2Buffer,
  0,
  sizeof(usart2Buffer),
  printLine
};
SerialHandle_t usart2   = {
  .device = USART2,
  .name = "usart2",
  .txPin = NULL,
  .rxPin = NULL,
  .interruptPin = NULL,
  .txStreamBuffer = NULL,
  .rxStreamBuffer = NULL,
  .txBufferSize = 64,
  .rxBufferSize = 128,
  .rxBytesFromISR = serialGenericRxBytesFromISR,
  .getTxBytesFromISR = serialGenericGetTxBytesFromISR,
  .processByte = processLineBufferedRxByte,
  .data = &usart2LineBuffer,
  .enabled = false,
  .flags = 0,
  .preTxCb = NULL,
  .postTxCb = NULL,
};
#endif

#ifdef DEBUG_USE_USART3
#define USART3_LINE_BUFF_LEN 512
static uint8_t usart3Buffer[USART3_LINE_BUFF_LEN];
SerialLineBuffer_t usart3LineBuffer = {
  usart3Buffer,
  0,
  sizeof(usart3Buffer),
  printLine
};
SerialHandle_t usart3   = {
  .device = USART3,
  .name = "usart3",
  .txPin = NULL,
  .rxPin = NULL,
  .interruptPin = NULL,
  .txStreamBuffer = NULL,
  .rxStreamBuffer = NULL,
  .txBufferSize = 64,
  .rxBufferSize = 64,
  .rxBytesFromISR = serialGenericRxBytesFromISR,
  .getTxBytesFromISR = serialGenericGetTxBytesFromISR,
  .processByte = processLineBufferedRxByte,
  .data = &usart3LineBuffer,
  .enabled = false,
  .flags = 0,
  .preTxCb = NULL,
  .postTxCb = NULL,
};
#endif

// List of pointers to all enabled serial handles
static SerialHandle_t *pserialInterfaces[] = {
#ifdef DEBUG_USE_LPUART1
&lpuart1,
#endif
#ifdef DEBUG_USE_USART1
&usart1,
#endif
#ifdef DEBUG_USE_USART2
&usart2,
#endif
#ifdef DEBUG_USE_USART3
&usart3,
#endif
};

static BaseType_t debugSerialCommand( char *writeBuffer,
                                        size_t writeBufferLen,
                                        const char *commandString);

static const CLI_Command_Definition_t cmdSerial = {
  // Command string
  "serial",
  // Help string
  "serial:\n"
  " * serial list\n"
  " * serial enable <interface>\n"
  " * serial tx <interface> <message>\n"
  " * serial baud <interface> <baudrate>\n"
  " * serial rxLevel <interface> <high/low>\n"
  " * serial disable <interface>\n",
  // Command function
  debugSerialCommand,
  // Number of parameters (variable)
  -1
};

SerialHandle_t* serialListInterfaces() {
  SerialHandle_t *handle = NULL;
  for(uint8_t idx = 0; idx < sizeof(pserialInterfaces)/sizeof(SerialHandle_t *); idx++) {
    printf("%s\n", pserialInterfaces[idx]->name);
  }
  return handle;
}

SerialHandle_t* serialFindInterface(const char *name, size_t len) {
  SerialHandle_t *handle = NULL;
  for(uint8_t idx = 0; idx < sizeof(pserialInterfaces)/sizeof(SerialHandle_t *); idx++) {
    if (strncmp(pserialInterfaces[idx]->name, name, len) == 0){
      handle = pserialInterfaces[idx];
      break;
    }
  }
  return handle;
}

static BaseType_t debugSerialCommand(char *writeBuffer,
                                      size_t writeBufferLen,
                                      const char *commandString) {
  const char *parameter;
  BaseType_t parameterStringLength;

  ( void ) writeBuffer;
  ( void ) writeBufferLen;

  do {
    parameter = FreeRTOS_CLIGetParameter(
                    commandString,
                    1, // Get the first parameter (command)
                    &parameterStringLength);

    if(parameter == NULL) {
      printf("ERR Invalid paramters\n");
      break;
    }

    if (strncmp("list", parameter, parameterStringLength) == 0) {
      serialListInterfaces();
    } else if (strncmp("enable", parameter, parameterStringLength) == 0) {
      parameter = FreeRTOS_CLIGetParameter(
                      commandString,
                      2, // Get the second parameter (name)
                      &parameterStringLength);
      SerialHandle_t *handle = serialFindInterface(parameter, parameterStringLength);

      if (handle == NULL) {
        printf("ERR Invalid interface\n");
        break;
      }

      serialEnable(handle);
      printf("OK\n");

    } else if (strncmp("disable", parameter, parameterStringLength) == 0) {
      parameter = FreeRTOS_CLIGetParameter(
                      commandString,
                      2, // Get the second parameter (name)
                      &parameterStringLength);
      SerialHandle_t *handle = serialFindInterface(parameter, parameterStringLength);

      if (handle == NULL) {
        printf("ERR Invalid interface\n");
        break;
      }

      serialDisable(handle);
      printf("OK\n");

    } else if (strncmp("tx", parameter, parameterStringLength) == 0) {
      parameter = FreeRTOS_CLIGetParameter(
                      commandString,
                      2, // Get the second parameter (name)
                      &parameterStringLength);
      SerialHandle_t *handle = serialFindInterface(parameter, parameterStringLength);

      if (handle == NULL) {
        printf("ERR Invalid interface\n");
        break;
      }

      parameter = FreeRTOS_CLIGetParameter(
                      commandString,
                      3, // Get the rest of the parameters
                      &parameterStringLength);

      SerialMessage_t debugUartMessage;
      debugUartMessage.len = strnlen(parameter, CONSOLE_OUTPUT_SIZE) + 2; // Added 2 characters for crlf
      debugUartMessage.buff = pvPortMalloc(debugUartMessage.len);
      debugUartMessage.destination = handle;
      configASSERT(debugUartMessage.buff != NULL);

      memcpy(debugUartMessage.buff, parameter, debugUartMessage.len);
      debugUartMessage.buff[debugUartMessage.len - 2] = '\r';
      debugUartMessage.buff[debugUartMessage.len - 1] = '\n';

      if(xQueueSend(serialGetTxQueue(), &debugUartMessage, 10 ) != pdTRUE) {
        // Free buffer if we were unable to send it
        vPortFree(debugUartMessage.buff);
      }
      printf("OK\n");
      // Transmit bytes
      // should have generic tx task for all interfaces?

    } else if (strncmp("rxLevel", parameter, parameterStringLength) == 0) {

      parameter = FreeRTOS_CLIGetParameter(
                      commandString,
                      2, // Get the second parameter (serial interface)
                      &parameterStringLength);
      SerialHandle_t *handle = serialFindInterface(parameter, parameterStringLength);

      if (handle == NULL) {
        printf("ERR Invalid interface\n");
        break;
      }

      parameter = FreeRTOS_CLIGetParameter(
                      commandString,
                      3, // Get the rest of the parameters
                      &parameterStringLength);

      LL_USART_Disable((USART_TypeDef *)handle->device);

      if (strncmp("high", parameter, parameterStringLength) == 0) {
        LL_USART_SetRXPinLevel((USART_TypeDef *)handle->device, LL_USART_RXPIN_LEVEL_STANDARD);
      } else if (strncmp("low", parameter, parameterStringLength) == 0) {
        LL_USART_SetRXPinLevel((USART_TypeDef *)handle->device, LL_USART_RXPIN_LEVEL_INVERTED);
      } else {
        printf("ERR Invalid paramters\n");
        break;
      }

      LL_USART_Enable((USART_TypeDef *)handle->device);
      printf("OK\n");
    } else if (strncmp("baud", parameter, parameterStringLength) == 0) {
      BaseType_t handle_string_len;
      const char* handle_string = FreeRTOS_CLIGetParameter(
                      commandString,
                      2, // Get the second parameter (serial interface)
                      &handle_string_len);
      SerialHandle_t *handle = serialFindInterface(handle_string, handle_string_len);

      if (handle == NULL) {
        printf("ERR Invalid interface\n");
        break;
      }

      parameter = FreeRTOS_CLIGetParameter(
                      commandString,
                      3, // Get the rest of the parameters
                      &parameterStringLength);

      uint32_t new_baud = atoi(parameter);
      if (new_baud == 0) {
        printf("ERR Invalid baud\n");
        break;
      }
      if (strncmp(handle_string, "lpuart", handle_string_len) == 0) {
        LL_LPUART_SetBaudRate((USART_TypeDef *)handle->device,
                        LL_RCC_GetLPUARTClockFreq(LL_RCC_LPUART1_CLKSOURCE),
                        LL_LPUART_PRESCALER_DIV64, new_baud);
      } else if (strncmp(handle_string, "usart1", handle_string_len) == 0) {
        // TODO - verify prescaler and oversampling
        LL_USART_SetBaudRate((USART_TypeDef *)handle->device,
                        LL_RCC_GetUSARTClockFreq(LL_RCC_USART1_CLKSOURCE),
                        LL_USART_PRESCALER_DIV1,
                        LL_USART_OVERSAMPLING_16,
                        new_baud);
      } else if (strncmp(handle_string, "usart2", handle_string_len) == 0) {
        // TODO - verify prescaler and oversampling
        LL_USART_SetBaudRate((USART_TypeDef *)handle->device,
                        LL_RCC_GetUSARTClockFreq(LL_RCC_USART2_CLKSOURCE),
                        LL_USART_PRESCALER_DIV1,
                        LL_USART_OVERSAMPLING_16,
                        new_baud);
      } else if (strncmp(handle_string, "usart3", handle_string_len) == 0) {
        LL_USART_SetBaudRate((USART_TypeDef *)handle->device,
                        LL_RCC_GetUSARTClockFreq(LL_RCC_USART3_CLKSOURCE),
                        LL_USART_PRESCALER_DIV1,
                        LL_USART_OVERSAMPLING_16,
                        new_baud);
      } else {
        printf("ERR Invalid paramters\n");
        break;
      }

      printf("OK\n");
    } else {
      printf("ERR Invalid paramters\n");
      break;
    }

  } while(0);
  return pdFALSE;
}

void startDebugUart() {
  BaseType_t rval;

  // UARTLog = logCreate("UART", "log", LOG_LEVEL_DEBUG, LOG_DEST_ALL);
  // logSetBuffSize(UARTLog, 1024);
  // logInit(UARTLog);

#ifdef DEBUG_USE_LPUART1
  // Single byte trigger level for fast response time
  lpuart1.txStreamBuffer = xStreamBufferCreate(lpuart1.txBufferSize, 1);
  configASSERT(lpuart1.txStreamBuffer != NULL);

  lpuart1.rxStreamBuffer = xStreamBufferCreate(lpuart1.rxBufferSize, 1);
  configASSERT(lpuart1.rxStreamBuffer != NULL);

  rval = xTaskCreate(
              serialGenericRxTask,
              "LPUartRx",
              // TODO - verify stack size
              configMINIMAL_STACK_SIZE,
              &lpuart1,
              DEBUG_UART_RX_TASK_PRIORITY,
              NULL);
  configASSERT(rval == pdTRUE);
#endif

#ifdef DEBUG_USE_USART1
  usart1.txStreamBuffer = xStreamBufferCreate(usart1.txBufferSize, 1);
  configASSERT(usart1.txStreamBuffer != NULL);

  usart1.rxStreamBuffer = xStreamBufferCreate(usart1.rxBufferSize, 1);
  configASSERT(usart1.rxStreamBuffer != NULL);

  rval = xTaskCreate(
              serialGenericRxTask,
              "Usart1Rx",
              // TODO - verify stack size
              configMINIMAL_STACK_SIZE,
              &usart1,
              DEBUG_UART_RX_TASK_PRIORITY,
              NULL);
  configASSERT(rval == pdTRUE);
#endif

#ifdef DEBUG_USE_USART2
  usart2.txStreamBuffer = xStreamBufferCreate(usart2.txBufferSize, 1);
  configASSERT(usart2.txStreamBuffer != NULL);

  usart2.rxStreamBuffer = xStreamBufferCreate(usart2.rxBufferSize, 1);
  configASSERT(usart2.rxStreamBuffer != NULL);

  rval = xTaskCreate(
              serialGenericRxTask,
              "Usart2Rx",
              // TODO - verify stack size
              configMINIMAL_STACK_SIZE,
              &usart2,
              DEBUG_UART_RX_TASK_PRIORITY,
              NULL);
  configASSERT(rval == pdTRUE);
#endif

#ifdef DEBUG_USE_USART3
  usart3.txStreamBuffer = xStreamBufferCreate(usart3.txBufferSize, 1);
  configASSERT(usart3.txStreamBuffer != NULL);

  usart3.rxStreamBuffer = xStreamBufferCreate(usart3.rxBufferSize, 1);
  configASSERT(usart3.rxStreamBuffer != NULL);

  rval = xTaskCreate(
              serialGenericRxTask,
              "Usart3Rx",
              // TODO - verify stack size
              configMINIMAL_STACK_SIZE * 2,
              &usart3,
              DEBUG_UART_RX_TASK_PRIORITY,
              NULL);
  configASSERT(rval == pdTRUE);
#endif

  FreeRTOS_CLIRegisterCommand( &cmdSerial );

}

static void processLineBufferedRxByte(void *serialHandle, uint8_t byte) {
  configASSERT(serialHandle != NULL);
  SerialHandle_t *handle = (SerialHandle_t *)serialHandle;

  // This function requires data to be a pointer to a SerialLineBuffer_t
  configASSERT(handle->data != NULL);

  SerialLineBuffer_t *lineBuffer = (SerialLineBuffer_t *)handle->data;

  // We need a buffer to use!
  configASSERT(lineBuffer->buffer != NULL);

  lineBuffer->buffer[lineBuffer->idx] = byte;

  if(lineBuffer->buffer[lineBuffer->idx] == '\n'){

    // Zero terminate the line
    if ((lineBuffer->idx + 1) < lineBuffer->len) {
      lineBuffer->buffer[lineBuffer->idx + 1] = 0;
    }

    if(lineBuffer->lineCallback != NULL) {
      lineBuffer->lineCallback(handle, lineBuffer->buffer, lineBuffer->idx);
    }

    // Reset buffer index
    lineBuffer->idx = 0;
  } else {

    lineBuffer->idx++;

    // Heavy handed way of dealing with overflow for now
    // Later we can just purge the buffer
    // TODO - log error and clear buffer instead
    configASSERT((lineBuffer->idx + 1) < lineBuffer->len);
  }
}

static void printLine(void *serialHandle, uint8_t *line, size_t len) {
  configASSERT(serialHandle != NULL);
  SerialHandle_t *handle = (SerialHandle_t *)serialHandle;

  configASSERT(line != NULL);

  // Not using len right now
  (void) len;

  // logPrint(UARTLog, LOG_LEVEL_DEBUG, "[%s] %s", handle->name, line);
  printf("[%s] %s", handle->name, line);
}

#ifdef DEBUG_USE_LPUART1
void LPUART1_IRQHandler(void) {
  serialGenericUartIRQHandler(&lpuart1);
}
#endif

#ifdef DEBUG_USE_USART1
void USART1_IRQHandler(void) {
  serialGenericUartIRQHandler(&usart1);
}
#endif

#ifdef DEBUG_USE_USART2
void USART2_IRQHandler(void) {
  serialGenericUartIRQHandler(&usart2);
}
#endif

#ifdef DEBUG_USE_USART3
void USART3_IRQHandler(void) {
  serialGenericUartIRQHandler(&usart3);
}
#endif
