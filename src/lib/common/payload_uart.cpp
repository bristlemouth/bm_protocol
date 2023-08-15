#include "payload_uart.h"
#include "uptime.h"
#include "bm_printf.h"
#include "stm32_rtc.h"

namespace PLUART {

// Line termination character
  static char terminationCharacter = 0;

  char getTerminationCharacter() {
    return terminationCharacter;
  }

  void setTerminationCharacter(char term_char) {
    terminationCharacter = term_char;
  }

// Called everytime we get a UART byte
  void processLineBufferedRxByte(void *serialHandle, uint8_t byte) {
    configASSERT(serialHandle != NULL);
    SerialHandle_t *handle = reinterpret_cast<SerialHandle_t *>(serialHandle);
    //  printf("%c", byte);
    // This function requires data to be a pointer to a SerialLineBuffer_t
    configASSERT(handle->data != NULL);
    SerialLineBuffer_t *lineBuffer = reinterpret_cast<SerialLineBuffer_t *>(handle->data);
    // We need a buffer to use!
    configASSERT(lineBuffer->buffer != NULL);
    lineBuffer->buffer[lineBuffer->idx] = byte;
    if(lineBuffer->buffer[lineBuffer->idx] == terminationCharacter){
      // Zero terminate the line
      if ((lineBuffer->idx + 1) < lineBuffer->len) {
        lineBuffer->buffer[lineBuffer->idx + 1] = 0;
      }
      if(lineBuffer->lineCallback != NULL) {
        lineBuffer->lineCallback(handle, lineBuffer->buffer, lineBuffer->idx);
      }
      // Reset buffer index
      lineBuffer->idx = 0;
    }
    else {
      lineBuffer->idx++;
      // Heavy handed way of dealing with overflow for now
      // Later we can just purge the buffer
      // TODO - log error and clear buffer instead
      configASSERT((lineBuffer->idx + 1) < lineBuffer->len);
    }
    if (userProcessRxByte) {
      userProcessRxByte(byte);
    }
  }

// Called everytime we get a 'line' by processLineBufferedRxByte
  void processLine(void *serialHandle, uint8_t *line, size_t len) {
    configASSERT(serialHandle != NULL);
    SerialHandle_t *handle = reinterpret_cast<SerialHandle_t *>(serialHandle);

    configASSERT(line != NULL);

    RTCTimeAndDate_t timeAndDate;
    char rtcTimeBuffer[32];
    if (rtcGet(&timeAndDate) == pdPASS) {
      sprintf(rtcTimeBuffer, "%04u-%02u-%02uT%02u:%02u:%02u.%03u",
              timeAndDate.year,
              timeAndDate.month,
              timeAndDate.day,
              timeAndDate.hour,
              timeAndDate.minute,
              timeAndDate.second,
              timeAndDate.ms);
    } else {
      strcpy(rtcTimeBuffer, "0");
    }

    bm_fprintf(0, "payload_data.log", "tick: %llu, rtc: %s, line: %.*s\n", uptimeGetMs(), rtcTimeBuffer, len, line);
    bm_printf(0, "[%s] | tick: %llu, rtc: %s, line: %.*s", handle->name, uptimeGetMs(), rtcTimeBuffer, len, line);
    printf("[%s] | tick: %llu, rtc: %s, line: %.*s\n", handle->name, uptimeGetMs(), rtcTimeBuffer, len, line);

    if (userProcessLine) {
      userProcessLine(line, len);
    }
  }

  void setBaud(uint32_t new_baud_rate) {
    LL_LPUART_SetBaudRate(static_cast<USART_TypeDef *>(uart_handle.device),
                          LL_RCC_GetLPUARTClockFreq(LL_RCC_LPUART1_CLKSOURCE),
                          LL_LPUART_PRESCALER_DIV64,
                          new_baud_rate);
  }

// variable definitions
  static uint8_t lpUart1Buffer[LPUART1_LINE_BUFF_LEN];
  SerialLineBuffer_t lpUART1LineBuffer = {
      .buffer = lpUart1Buffer, // pointer to the buffer memory
      .idx = 0, // variable to store current index in the buffer
      .len = sizeof(lpUart1Buffer), // total size of buffer
      .lineCallback = processLine // lineCallback callback function to call when a line is complete (glued together in the UART handle's byte callback).
  };
  SerialHandle_t uart_handle = {
      .device = LPUART1,
      .name = "payload",
      .txPin = &PAYLOAD_TX,
      .rxPin = &PAYLOAD_RX,
      .txStreamBuffer = NULL,
      .rxStreamBuffer = NULL,
      .txBufferSize = 128,
      .rxBufferSize = 2048,
      .rxBytesFromISR = serialGenericRxBytesFromISR,
      .getTxBytesFromISR = serialGenericGetTxBytesFromISR,
      .processByte = processLineBufferedRxByte, // This is where we tell it the callback to call when we get a new byte
      .data = &lpUART1LineBuffer, // Pointer to the line buffer this handle should use
      .enabled = false,
      .flags = 0,
  };

  BaseType_t initPayloadUart(uint8_t task_priority) {
    MX_LPUART1_UART_Init();
    // Single byte trigger levels for Rx and Tx for fast response time
    uart_handle.txStreamBuffer = xStreamBufferCreate(uart_handle.txBufferSize, 1);
    configASSERT(uart_handle.txStreamBuffer != NULL);
    uart_handle.rxStreamBuffer = xStreamBufferCreate(uart_handle.rxBufferSize, 1);
    configASSERT(uart_handle.rxStreamBuffer != NULL);
    BaseType_t rval = xTaskCreate(
        serialGenericRxTask,
        "LPUartRx",
        // TODO - verify stack size
        2048,
        &PLUART::uart_handle,
        task_priority,
        NULL);
    configASSERT(rval == pdTRUE);

    return rval;
  }

  extern "C" void LPUART1_IRQHandler(void) {
    serialGenericUartIRQHandler(&uart_handle);
  }

}  // namespace PLUART
