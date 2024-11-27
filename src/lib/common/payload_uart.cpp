#include "payload_uart.h"
#include "spotter.h"
#include "stm32_rtc.h"
#include "uptime.h"
#include "stm32_io.h"

// FreeRTOS Includes
#include "FreeRTOS.h"
#include "semphr.h"

namespace PLUART {

#define USER_BYTE_BUFFER_LEN 2048
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
// Flag to use lineBuffer or not. If not, user can pull bytes directly from the stream.
static bool _useLineBuffer = true;
// Flag to use xStreamBufferIsFull or not. If not, user can use LineBuffer.
static bool _useByteStreamBuffer = false;

// Flag to track if a multi-write transaction is in-progress, useful for eg HALF_DUPLEX
static bool _transactionInProgress = false;
// Track if a write occurred during a transaction, to ensure _postTxFunction always called
static bool _writeDuringTransaction = false;
// Optional transaction start function
static HardwareControlFunc _preTxFunction = nullptr;
// Optional transaction end function, will be called after final write of transaction completes
static HardwareControlFunc _postTxFunction = nullptr;
// Semaphore for passing token between postTxCallback, endTransaction, and write to ensure
//  proper synchronization between completion of all writes in the transaction blocking _postTxFunction.
static SemaphoreHandle_t _postTxSemaphore = nullptr;

// Forward declaration of the post-transmission callback function
static void pluartPostTransactionCb(SerialHandle_t *handle);

// Set transaction hardware control functions
void enableTransactions(HardwareControlFunc preTxFunc, HardwareControlFunc postTxFunc) {
  // Assign the transaction start and end callbacks
  _preTxFunction = preTxFunc;
  _postTxFunction = postTxFunc;
  // Assign the post-transaction callback function,
  //  which wraps _postTxFunction in some additional checks to ensure transaction is complete.
  uart_handle.postTxCb = pluartPostTransactionCb;
}

// Start a Tx transaction
// TODO - return false if transaction already started?
void startTransaction() {
  _transactionInProgress = true;
  _writeDuringTransaction = false;
  if (_preTxFunction) {
    _preTxFunction(); // External start transaction function. Eg, enable Tx for Half-Duplex
  }
}

/** End a Tx transaction, called by user.
// Blocks until all PLUART writes are complete then fires _postTxFunction
// Description: First safely check if there's a transaction to end, safely wait for all Tx writing
                to finish, then _postTxFunction().
TODO - If users have multiple tasks interacting with PLUART transactions,
       this will heavily bork. This is not a currently supported use case.
 **/
bool endTransaction(uint32_t wait_ms) {
  // If not in a transaction, bail and tell the user.
  if (!_transactionInProgress) {
    return false;
  }
  // transaction is in progress, and need to call _postTxFunction when writing is complete.
  _transactionInProgress = false;
  // If there were no writes during the transaction, don't wait for writes to complete.
  if (!_writeDuringTransaction) {
    if (_postTxFunction != nullptr) _postTxFunction(); // Safely call the function
    return true;
  }
  // After each transmission compeltes, TC interrupt will call pluartPostTransactionCb,
  //    which will give a token to the semaphore.
  // Each subsequent write inside of the transaction will take the token,
  //    so only the completion of the final Tx transmission will leave a token available here.
  bool ret_value = true;
  if (xSemaphoreTake(_postTxSemaphore, pdMS_TO_TICKS(wait_ms)) == pdFALSE) {
    // Failed to take sempaphore within transaction timeout
    ret_value = false; // TODO - maybe better to assert(false) here?
  }
  // Call the post-transaction function after the final transmission is fully complete
  if (_postTxFunction != nullptr) _postTxFunction();
  return ret_value;
}

// Post-transmission callback function, attached as uart_handle.postTxCb and
// *called from ISR* when the TC interrupt fires [see common/serial.c::serialGenericUartIRQHandler].
static void pluartPostTransactionCb(SerialHandle_t *handle) {
  (void)handle; // Mark 'handle' as intentionally unused

  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  // Awaiting a token in this semaphore is used to block endTransaction from calling _postTxFunction.
  // Every call to write takes the token if present, every complete uart transmission gives it.
  xSemaphoreGiveFromISR(_postTxSemaphore, &xHigherPriorityTaskWoken);
  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// Process a received byte and store it in a line buffer
static void processLineBufferedRxByte(void *serialHandle, uint8_t byte) {
  configASSERT(serialHandle != NULL);
  SerialHandle_t *handle = reinterpret_cast<SerialHandle_t *>(serialHandle);
  if (_useLineBuffer) {
    // This function requires data to be a pointer to a SerialLineBuffer_t
    configASSERT(handle->data != NULL);
    SerialLineBuffer_t *lineBuffer = reinterpret_cast<SerialLineBuffer_t *>(handle->data);
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
      //    printf("buffer reset!\n");
    } else {
      lineBuffer->idx++;
      // Heavy handed way of dealing with overflow for now
      // Later we can just purge the buffer
      // TODO - log error and clear buffer instead
      configASSERT((lineBuffer->idx + 1) < lineBuffer->len);
    }
  }

  if (_useByteStreamBuffer) {
    // Send the byte to the user byte stream buffer, if it is full, reset it first
    if (xStreamBufferIsFull(user_byte_stream_buffer) == pdTRUE) {
      printf("WARN payload uart user byte stream buffer full!\n");
      xStreamBufferReset(user_byte_stream_buffer);
    }
    xStreamBufferSend(user_byte_stream_buffer, &byte, sizeof(byte), 0);
  }
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

void setTerminationCharacter(char term_char) { terminationCharacter = term_char; }

bool getUseLineBuffer() { return _useLineBuffer; }

void setUseLineBuffer(bool enable) { _useLineBuffer = enable; }

bool getUseByteStreamBuffer() { return _useByteStreamBuffer; }

void setUseByteStreamBuffer(bool enable) { _useByteStreamBuffer = enable; }

bool byteAvailable(void) { return (!xStreamBufferIsEmpty(user_byte_stream_buffer)); }

bool lineAvailable(void) {
  bool rval = false;
  configASSERT(xSemaphoreTake(_user_line.mutex, portMAX_DELAY) == pdTRUE);
  rval = _user_line.ready;
  xSemaphoreGive(_user_line.mutex);
  return rval;
}

void reset(void) {
  disable(); // Disable the UART
  configASSERT(xSemaphoreTake(_user_line.mutex, portMAX_DELAY) == pdTRUE);
  // Clear the line buffer state
  memset(_user_line.buffer, 0, LPUART1_LINE_BUFF_LEN);
  _user_line.len = 0;
  _user_line.ready = false;
  xSemaphoreGive(_user_line.mutex);
  // Clear the user byte stream buffer
  xStreamBufferReset(user_byte_stream_buffer);
  enable(); // Reeable the UART
}

uint8_t readByte(void) {
  uint8_t rval = 0;
  xStreamBufferReceive(user_byte_stream_buffer, &rval, sizeof(rval), 0);
  return rval;
}

uint16_t readLine(char *buffer, size_t len) {
  int16_t rval = 0;
  size_t copy_len;

  configASSERT(xSemaphoreTake(_user_line.mutex, portMAX_DELAY) == pdTRUE);
  if (len > _user_line.len) {
    copy_len = _user_line.len;
  } else {
    copy_len = len;
  }

  memcpy(buffer, _user_line.buffer, copy_len);
  memset(_user_line.buffer, 0, LPUART1_LINE_BUFF_LEN);
  _user_line.ready = false;
  rval = copy_len;
  xSemaphoreGive(_user_line.mutex);
  return rval;
}

// TODO - change to bool and return false if transactions are enabled, but no transaciton is in progress?
void write(uint8_t *buffer, size_t len) {
  // If we are using transaction, need to flag that we've had at least
  //  one write in the transaction so we don't trigger postTx callback prematurely.
  //  see endTransaction()
  if (_transactionInProgress) {
    _writeDuringTransaction = true;
  }
  // Token being available in the semaphore here means TC has already fired in a transation,
  //  and this write is starting a subsequent transmission in the transaction. Take the token so
  //  endTransaction has to wait for this transmission to complete.
  // Note - multiple writes will likely be grouped in the same UART Tx transmission if there is no
  //  delay between them in the transaction in the user thread.
  xSemaphoreTake(_postTxSemaphore, 0);
  serialWrite(&PLUART::uart_handle, buffer, len);
}

void setBaud(uint32_t new_baud_rate) {
  LL_LPUART_SetBaudRate(static_cast<USART_TypeDef *>(uart_handle.device),
                        LL_RCC_GetLPUARTClockFreq(LL_RCC_LPUART1_CLKSOURCE),
                        LL_LPUART_PRESCALER_DIV64, new_baud_rate);
}

void setEvenParity(void){
  LL_LPUART_Disable(static_cast<USART_TypeDef *>(uart_handle.device));
  LL_LPUART_SetParity(static_cast<USART_TypeDef *>(uart_handle.device),
                      LL_LPUART_PARITY_EVEN);
  LL_LPUART_Enable(static_cast<USART_TypeDef *>(uart_handle.device));
}

void enableDataInversion(void){
  LL_LPUART_Disable(static_cast<USART_TypeDef *>(uart_handle.device));
  LL_LPUART_SetTXPinLevel(static_cast<USART_TypeDef *>(uart_handle.device),
                       LL_LPUART_TXPIN_LEVEL_INVERTED);
  LL_LPUART_Enable(static_cast<USART_TypeDef *>(uart_handle.device));
}

// Configuring TX as a GPIO
void setTxPinOutputLevel(void) {
  if(uart_handle.txPin) {
    STM32Pin_t *pin = (STM32Pin_t *)uart_handle.txPin->pin;
    LL_GPIO_SetOutputPin((GPIO_TypeDef *)pin->gpio, pin->pinmask);
  }
}

void resetTxPinOutputLevel(void){
  if(uart_handle.txPin) {
    STM32Pin_t *pin = (STM32Pin_t *)uart_handle.txPin->pin;
    LL_GPIO_ResetOutputPin((GPIO_TypeDef *)pin->gpio, pin->pinmask);
  }
}

void configTxPinOutput(void){
  if(uart_handle.txPin) {
    STM32Pin_t *pin = (STM32Pin_t *)uart_handle.txPin->pin;
    LL_GPIO_SetPinMode((GPIO_TypeDef *)pin->gpio, pin->pinmask, LL_GPIO_MODE_OUTPUT);
  }
}

void configTxPinAlternate(void){
  if(uart_handle.txPin) {
    STM32Pin_t *pin = (STM32Pin_t *)uart_handle.txPin->pin;
    LL_GPIO_SetPinMode((GPIO_TypeDef *)pin->gpio, pin->pinmask, LL_GPIO_MODE_ALTERNATE);
  }
}

void enable(void) { serialEnable(&uart_handle); }

void disable(void) { serialDisable(&uart_handle); }

// variable definitions
static uint8_t lpUart1Buffer[LPUART1_LINE_BUFF_LEN];
SerialLineBuffer_t lpUART1LineBuffer = {
    .buffer = lpUart1Buffer,      // pointer to the buffer memory
    .idx = 0,                     // variable to store current index in the buffer
    .len = sizeof(lpUart1Buffer), // total size of buffer
    .lineCallback =
        processLine // lineCallback callback function to call when a line is complete (glued together in the UART handle's byte callback).
};
SerialHandle_t uart_handle = {
    .device = LPUART1,
    .name = "payload",
    .txPin = &PAYLOAD_TX,
    .rxPin = &PAYLOAD_RX,
    .interruptPin = NULL,
    .txStreamBuffer = NULL,
    .rxStreamBuffer = NULL,
    .txBufferSize = 128,
    .rxBufferSize = 2048,
    .rxBytesFromISR = serialGenericRxBytesFromISR,
    .getTxBytesFromISR = serialGenericGetTxBytesFromISR,
    .processByte =
        processLineBufferedRxByte, // This is where we tell it the callback to call when we get a new byte
    .data = &lpUART1LineBuffer, // Pointer to the line buffer this handle should use
    .enabled = false,
    .flags = 0,
    .preTxCb = NULL,
    .postTxCb = NULL,
};

BaseType_t init(uint8_t task_priority) {
  // Create the stream buffer for the user bytes to be buffered into and read from
  user_byte_stream_buffer = xStreamBufferCreate(USER_BYTE_BUFFER_LEN, 1);
  configASSERT(user_byte_stream_buffer != NULL);

  // Create the mutex used by the payload_uart namespace for users to safely access lines from the payload
  _user_line.mutex = xSemaphoreCreateMutex();
  configASSERT(_user_line.mutex != NULL);

  // Create the postTxFunction callback trigger semaphore
  _postTxSemaphore = xSemaphoreCreateBinary();
  configASSERT(_postTxSemaphore != NULL);

  MX_LPUART1_UART_Init();
  // Single byte trigger levels for Rx and Tx for fast response time
  uart_handle.txStreamBuffer = xStreamBufferCreate(uart_handle.txBufferSize, 1);
  configASSERT(uart_handle.txStreamBuffer != NULL);
  uart_handle.rxStreamBuffer = xStreamBufferCreate(uart_handle.rxBufferSize, 1);
  configASSERT(uart_handle.rxStreamBuffer != NULL);
  BaseType_t rval = xTaskCreate(serialGenericRxTask, "LPUartRx",
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
