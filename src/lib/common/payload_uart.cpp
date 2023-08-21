#include "payload_uart.h"
#include "bm_printf.h"
#include "stm32_rtc.h"
#include "uptime.h"

// FreeRTOS Includes
#include "FreeRTOS.h"
#include "semphr.h"

namespace PLUART {

#define USER_BYTE_BUFFER_LEN 128
// Stream buffer for user bytes
static StreamBufferHandle_t user_byte_stream_buffer = NULL;

static struct {
  // This mutex protects the buffer, len, and ready flag
  SemaphoreHandle_t mutex;
  uint8_t buffer[LPUART1_LINE_BUFF_LEN];
  uint16_t len = 0;
  bool ready = false;
} _user_line;

// Line termination character
static char terminationCharacter = 0;

// Process a received byte and store it in a line buffer
static void processLineBufferedRxByte(void *serialHandle, uint8_t byte) {
  configASSERT(serialHandle != NULL);
  SerialHandle_t *handle = reinterpret_cast<SerialHandle_t *>(serialHandle);
  // This function requires data to be a pointer to a SerialLineBuffer_t
  configASSERT(handle->data != NULL);
  SerialLineBuffer_t *lineBuffer =
      reinterpret_cast<SerialLineBuffer_t *>(handle->data);
  // We need a buffer to use!
  configASSERT(lineBuffer->buffer != NULL);
  lineBuffer->buffer[lineBuffer->idx] = byte;
  if (lineBuffer->buffer[lineBuffer->idx] == terminationCharacter) {
    // Zero terminate the line
    if ((lineBuffer->idx + 1) < lineBuffer->len) {
      lineBuffer->buffer[lineBuffer->idx + 1] = 0;
    }
    if (lineBuffer->lineCallback != NULL) {
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

  // Send the byte to the user byte stream buffer, if it is full, reset it first
  if (xStreamBufferIsFull(user_byte_stream_buffer) == pdTRUE) {
    xStreamBufferReset(user_byte_stream_buffer);
  }
  xStreamBufferSend(user_byte_stream_buffer, &byte, sizeof(byte), 0);
}

// Called everytime we get a 'line' by processLineBufferedRxByte
static void processLine(void *serialHandle, uint8_t *line, size_t len) {
  configASSERT(serialHandle != NULL);
  configASSERT(line != NULL);
  (void)serialHandle;

  configASSERT(xSemaphoreTake(_user_line.mutex, portMAX_DELAY) == pdTRUE);
  // if the last line has not been read, lets reset the buffer
  memset(_user_line.buffer, 0, LPUART1_LINE_BUFF_LEN);
  // lets copy over the line to the user mutex protected buffer
  memcpy(_user_line.buffer, line, len);
  _user_line.len = len;
  _user_line.ready = true;
  xSemaphoreGive(_user_line.mutex);
}

char getTerminationCharacter() { return terminationCharacter; }

void setTerminationCharacter(char term_char) {
  terminationCharacter = term_char;
}

bool byteAvailable(void) {
  return (!xStreamBufferIsEmpty(user_byte_stream_buffer));
}

bool lineAvailable(void) {
  bool rval = false;
  configASSERT(xSemaphoreTake(_user_line.mutex, portMAX_DELAY) == pdTRUE);
  rval = _user_line.ready;
  xSemaphoreGive(_user_line.mutex);
  return rval;
}

uint8_t readByte(void) {
  uint8_t rval = 0;
  xStreamBufferReceive(user_byte_stream_buffer, &rval, sizeof(rval), 0);
  return rval;
}

uint16_t readLine(char *buffer, size_t len) {
  int16_t rval = 0;
  size_t copy_len;

  if (len > _user_line.len) {
    copy_len = _user_line.len;
  } else {
    copy_len = len;
  }

  configASSERT(xSemaphoreTake(_user_line.mutex, portMAX_DELAY) == pdTRUE);
  memcpy(buffer, _user_line.buffer, copy_len);
  memset(_user_line.buffer, 0, LPUART1_LINE_BUFF_LEN);
  _user_line.ready = false;
  rval = copy_len;
  xSemaphoreGive(_user_line.mutex);
  return rval;
}

void write(uint8_t *buffer, size_t len) {
  serialWrite(&PLUART::uart_handle, buffer, len);
}

void setBaud(uint32_t new_baud_rate) {
  LL_LPUART_SetBaudRate(static_cast<USART_TypeDef *>(uart_handle.device),
                        LL_RCC_GetLPUARTClockFreq(LL_RCC_LPUART1_CLKSOURCE),
                        LL_LPUART_PRESCALER_DIV64, new_baud_rate);
}

void enable(void) { serialEnable(&uart_handle); }

// variable definitions
static uint8_t lpUart1Buffer[LPUART1_LINE_BUFF_LEN];
SerialLineBuffer_t lpUART1LineBuffer = {
    .buffer = lpUart1Buffer, // pointer to the buffer memory
    .idx = 0,                // variable to store current index in the buffer
    .len = sizeof(lpUart1Buffer), // total size of buffer
    .lineCallback =
        processLine // lineCallback callback function to call when a line is complete (glued together in the UART handle's byte callback).
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
    .processByte =
        processLineBufferedRxByte, // This is where we tell it the callback to call when we get a new byte
    .data =
        &lpUART1LineBuffer, // Pointer to the line buffer this handle should use
    .enabled = false,
    .flags = 0,
};

BaseType_t init(uint8_t task_priority) {
  // Create the stream buffer for the user bytes to be buffered into and read from
  user_byte_stream_buffer = xStreamBufferCreate(USER_BYTE_BUFFER_LEN, 1);
  configASSERT(user_byte_stream_buffer != NULL);

  // Create the mutex used by the payload_uart namespace for users to safely access lines from the payload
  _user_line.mutex = xSemaphoreCreateMutex();
  configASSERT(_user_line.mutex != NULL);

  MX_LPUART1_UART_Init();
  // Single byte trigger levels for Rx and Tx for fast response time
  uart_handle.txStreamBuffer = xStreamBufferCreate(uart_handle.txBufferSize, 1);
  configASSERT(uart_handle.txStreamBuffer != NULL);
  uart_handle.rxStreamBuffer = xStreamBufferCreate(uart_handle.rxBufferSize, 1);
  configASSERT(uart_handle.rxStreamBuffer != NULL);
  BaseType_t rval =
      xTaskCreate(serialGenericRxTask, "LPUartRx",
                  // TODO - verify stack size
                  2048, &PLUART::uart_handle, task_priority, NULL);
  configASSERT(rval == pdTRUE);

  return rval;
}

extern "C" void LPUART1_IRQHandler(void) {
  configASSERT(&uart_handle);
  serialGenericUartIRQHandler(&uart_handle);
}

} // namespace PLUART
