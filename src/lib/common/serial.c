#include <string.h>
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"
#include "stream_buffer.h"
#include "queue.h"

#include "bsp.h"

// #include "log.h"
#include "bm_usart.h"
#include "serial.h"
#include "stm32_io.h"
#include "task_priorities.h"
#include "tusb.h"
#include "app_util.h"

#include "lpm.h"

// Number of buffers to queue up for transmission
#define SERIAL_TX_QUEUE_SIZE 128

// Queue for all serial outputs
static xQueueHandle serialTxQueue = NULL;

xQueueHandle serialGetTxQueue() {
  return serialTxQueue;
}

void startSerial() {
  BaseType_t rval;

  serialTxQueue = xQueueCreate(SERIAL_TX_QUEUE_SIZE, sizeof(SerialMessage_t));
  configASSERT(serialTxQueue != NULL);

  rval = xTaskCreate(
              serialTxTask,
              "SerialTxTask",
              // TODO - verify stack size
              configMINIMAL_STACK_SIZE * 3,
              serialTxQueue,
              SERIAL_TX_TASK_PRIORITY,
              NULL);

  configASSERT(rval == pdTRUE);
}

// Receive character from uart rx interrupt and place in stream buffer
// to be processed in serialGenericRxTask
BaseType_t serialGenericRxBytesFromISR(SerialHandle_t *handle, uint8_t *buffer, size_t len) {
  BaseType_t higherPriorityTaskWoken = pdFALSE;

  configASSERT(handle != NULL);
  configASSERT(handle->rxStreamBuffer != NULL);
  configASSERT(buffer != NULL);
  configASSERT(len > 0);

#ifdef TRACE_SERIAL
  for(uint32_t byte=0; byte < len; byte++) {
    traceAddSerial(handle, buffer[byte], false, true);
  }
#endif

  size_t bytesSent = xStreamBufferSendFromISR(
                        handle->rxStreamBuffer,
                        buffer,
                        len,
                        &higherPriorityTaskWoken);

  if(bytesSent != len) {
    // Flag that some bytes were dropped
    handle->flags |= SERIAL_FLAG_RXDROP;

  }

  return higherPriorityTaskWoken;
}

size_t serialGenericGetTxBytes(SerialHandle_t *handle, uint8_t *buffer, size_t len) {
  size_t bytesAvailable = 0;

  configASSERT(handle != NULL);

  if(!xStreamBufferIsEmpty(handle->txStreamBuffer)) {
    bytesAvailable = xStreamBufferReceive(
                          handle->txStreamBuffer,
                          buffer,
                          len,
                          0); // no timeout
#ifdef TRACE_SERIAL
    for(uint32_t byte=0; byte < bytesAvailable; byte++) {
      traceAddSerial(handle, buffer[byte], true, false);
    }
#endif
  }

  return bytesAvailable;
}

size_t serialGenericGetTxBytesFromISR(SerialHandle_t *handle, uint8_t *buffer, size_t len) {
  size_t bytesAvailable = 0;

  configASSERT(handle != NULL);

  if(!xStreamBufferIsEmpty(handle->txStreamBuffer)) {
    bytesAvailable = xStreamBufferReceiveFromISR(
                            handle->txStreamBuffer,
                            buffer,
                            len,
                            NULL); // higher priority task wake?

#ifdef TRACE_SERIAL
    for(uint32_t byte=0; byte < bytesAvailable; byte++) {
      traceAddSerial(handle, buffer[byte], true, true);
    }
#endif
  }

  return bytesAvailable;
}

// Task to receive and process data from debug UART
void serialGenericRxTask( void *parameters ) {
  configASSERT(parameters != NULL);
  SerialHandle_t *handle = (SerialHandle_t *)parameters;

  // Make sure we have a strem buffer to receive data from
  configASSERT(handle->rxStreamBuffer != NULL);

  for(;;) {
    uint8_t byte;
    size_t rxLen = xStreamBufferReceive( handle->rxStreamBuffer,      // Stream buffer
                                          &byte,                      // Where to put the data
                                          sizeof(byte),               // Size of transfer
                                          portMAX_DELAY);             // Timeout (forever)

#ifdef TRACE_SERIAL
  traceAddSerial(handle, byte, false, false);
#endif

    if(handle->flags & SERIAL_FLAG_RXDROP) {
      handle->flags &= ~SERIAL_FLAG_RXDROP;
    }

    // Do something with received bytes if needed
    if(rxLen != 0 && handle->processByte != NULL) {
      handle->processByte(handle, byte);
    }
  }
}

void serialEnable(SerialHandle_t *handle) {
  configASSERT(handle != NULL);

#ifndef NO_UART
  // Don't do uart specific stuff for USB :D
  if(!HANDLE_IS_USB(handle)) {
    // Check for and clear any overrun flags
    if(usart_IsActiveFlag_ORE((USART_TypeDef *)handle->device)) {
      usart_ClearFlag_ORE((USART_TypeDef *)handle->device);
    }

    if(handle->txPin) {
      STM32Pin_t *pin = (STM32Pin_t *)handle->txPin->pin;
      LL_GPIO_SetPinMode((GPIO_TypeDef *)pin->gpio, pin->pinmask, LL_GPIO_MODE_ALTERNATE);
    }

    // Clear any data that might be in the rx buffer
    (void)usart_ReceiveData8((USART_TypeDef *)handle->device);


    // Enable Uart RX interrupt
    usart_EnableIT_RXNE((USART_TypeDef *)handle->device);
  }
#endif

  handle->enabled = true;
}

void serialDisable(SerialHandle_t *handle) {
  configASSERT(handle);

#ifndef NO_UART
  // Don't do uart specific stuff for USB :D
  if(!HANDLE_IS_USB(handle)) {
    // Disable Uart RX interrupt
    usart_DisableIT_RXNE((USART_TypeDef *)handle->device);


    if(handle->txPin) {
      STM32Pin_t *pin = (STM32Pin_t *)handle->txPin->pin;
      LL_GPIO_SetPinMode((GPIO_TypeDef *)pin->gpio, pin->pinmask, LL_GPIO_MODE_INPUT);
    }
  }
#endif

  handle->enabled = false;
}


#ifndef NO_UART
// Generic interrupt handler for all USART/UART/LPUART peripherals
// Takes an SerialHandle_t and receives serial data into a stream buffer
// as well as takes data from another stream buffer to send
// This function is meant to be called from all the USARTx_IRQHandler functions
void serialGenericUartIRQHandler(SerialHandle_t *handle) {
  BaseType_t higherPriorityTaskWoken = pdFALSE;

  size_t bytesAvailable = 0;

  configASSERT(handle != NULL);

  // Process received bytes
  if( usart_IsActiveFlag_RXNE((USART_TypeDef *)handle->device) &&
      usart_IsEnabledIT_RXNE((USART_TypeDef *)handle->device)){

    uint8_t byte = usart_ReceiveData8((USART_TypeDef *)handle->device);
    configASSERT(handle->rxBytesFromISR);
    higherPriorityTaskWoken = handle->rxBytesFromISR(handle, &byte, 1);
  }

  // TXE will set when Tx Data Reg (TDR) is empty. Process next byte to transmit
  if( usart_IsActiveFlag_TXE((USART_TypeDef *)handle->device) &&
      usart_IsEnabledIT_TXE((USART_TypeDef *)handle->device)) {
    uint8_t txByte;
    configASSERT(handle->getTxBytesFromISR);
    bytesAvailable = handle->getTxBytesFromISR(handle, &txByte, 1);

    if (bytesAvailable > 0) {
      // Transmit current byte
      usart_TransmitData8((USART_TypeDef *)handle->device, txByte);
    } else {
      // Disable this interrupt if there are no more bytes to transmit
      usart_DisableIT_TXE((USART_TypeDef *)handle->device);
    }
  }

  // Check if transmission just completed, and clear TC flag if so. TC will set when TDR and shift register are empty.
  if (!bytesAvailable && LL_USART_IsActiveFlag_TC((USART_TypeDef *)handle->device) && !usart_IsEnabledIT_TXE((USART_TypeDef *)handle->device)) {
      LL_USART_ClearFlag_TC((USART_TypeDef *)handle->device);
      // If have a postTxCb, call it.
      if(handle->postTxCb){
        handle->postTxCb(handle);
      }
  }

  // Handle UART overrun error
  if (usart_IsActiveFlag_ORE((USART_TypeDef *)handle->device)) {
    // TODO - maybe differentiate overrun error from rx buffer full
    handle->flags |= SERIAL_FLAG_RXDROP;
    usart_ClearFlag_ORE((USART_TypeDef *)handle->device);
  }

  // Let the RTOS know if a task needs to be woken up
  portYIELD_FROM_ISR(higherPriorityTaskWoken);
}
#endif

// Cheat by using the length field to send single characters to the queue
// This way we save a malloc/free
// Function used to echo back characters and print single character things
// Like the console '> '
extern xQueueHandle serialTxQueue; // TODO - don't extern this, put in handle or store otherwise
void serialPutcharUnbuffered(SerialHandle_t *handle, char character) {
  SerialMessage_t singleCharMessage = {NULL, 0xFF00 | (uint16_t)character, handle};
  xQueueSend( serialTxQueue, &singleCharMessage, 10 );
}

static void serialGenericTx(SerialHandle_t *handle, uint8_t *data, size_t len) {
  // Print out message in any connected interface
  configASSERT(handle != NULL);

  size_t totalBytesSent = 0;

#ifdef TRACE_SERIAL
  if(handle->enabled){
    for(uint32_t byte=0; byte < len; byte++) {
      traceAddSerial(handle, data[byte], true, false);
    }
  }
#endif
  if(handle->preTxCb){
    handle->preTxCb(handle);
  }
  uint32_t startTime = xTaskGetTickCount();

  // Make sure we properly handle buffers larger than the streamBuffer
  // totalBytesSent < len takes care of the len == 0 case as well
  // Timeout just in case...
  while (handle->enabled &&
          (totalBytesSent < len) &&
          timeRemainingTicks(startTime, pdMS_TO_TICKS(MAX_TX_TIME_MS)))
  {
    size_t bytesSent = xStreamBufferSend(
                              handle->txStreamBuffer,
                              &data[totalBytesSent],
                              (len - totalBytesSent),
                              10  // 10 tick maximum wait for send
                              );

    totalBytesSent += bytesSent;


    // Right now, the USB handle is the only one without a device pointer
    // TODO - use some sort of flags instead
    if(HANDLE_IS_USB(handle)){
      if(!(handle->flags & SERIAL_TX_IN_PROGRESS)) {
        // Start transmitting over USB
        tud_cdc_tx_complete_cb(HANDLE_CDC_ITF(handle));
      }
    } else {
#ifndef NO_UART
      // Enable transmit interrupt if not already transmitting
      //  When first enabled, this will trigger the interrupt and its IRQ handler because TDR will be empty.
      if(!usart_IsEnabledIT_TXE((USART_TypeDef *)handle->device)) {
        usart_EnableIT_TXE((USART_TypeDef *)handle->device);
      // Enable TC interrupt to detect end of transmission
        LL_USART_EnableIT_TC((USART_TypeDef *)handle->device);
      }

#endif
    }
  }

}

//
// Generic task to transmit serial data on a particular interface
//
void serialTxTask( void *parameters ) {
  configASSERT(parameters!= NULL);
  xQueueHandle txQueueHandle = (xQueueHandle)parameters;

  for(;;) {
    SerialMessage_t message;
    BaseType_t rval = xQueueReceive(txQueueHandle, &message, portMAX_DELAY);
    configASSERT(rval == pdTRUE);

    configASSERT(message.destination != NULL);
    SerialHandle_t *destination = (SerialHandle_t *)message.destination;

    // Handle special case for single bytes
    if ((message.buff == NULL) && ((message.len & 0xFF00) == 0xFF00)) {
      uint8_t single_byte;
      single_byte = (uint8_t)(message.len & 0xFF);
      serialGenericTx(destination, &single_byte, sizeof(uint8_t));
    } else if(message.buff != NULL) {
      serialGenericTx(destination, message.buff, message.len);
      vPortFree(message.buff);
    } else {
      // Null buffer (not on purpose)
      configASSERT(0);
    }
  }
}

/*!
  \fn void serialWrite(SerialHandle_t *handle, uint8_t *buff, size_t len)
  \brief Write bytes to serial device (buffer is copied)
  \param *handle destination serial device handle
  \param *buff buffer of data to write
  \param len buffer length
*/
void serialWrite(SerialHandle_t *handle, const uint8_t *buff, size_t len) {
  configASSERT(handle != NULL);
  configASSERT(buff != NULL);

  uint8_t *txBuff = pvPortMalloc(len);
  configASSERT(txBuff != NULL);

  memcpy(txBuff, buff, len);

  serialWriteNocopy(handle, txBuff, len);
}

/*!
  \fn void serialWrite(SerialHandle_t *handle, uint8_t *buff, size_t len)
  \brief Write bytes to serial device (buffer is not copied)
  \param *handle destination serial device handle
  \param *buff buffer of data to write (must be malloc'd)
  \param len buffer length

  Sending task will free buff, therefore caller should not free buffer
  or use data from stack (or static)
*/
void serialWriteNocopy(SerialHandle_t *handle, uint8_t *buff, size_t len) {
  configASSERT(handle != NULL);
  configASSERT(buff != NULL);

  SerialMessage_t serialWriteMessage = {
    .buff = (uint8_t *)buff,
    .len = len,
    .destination = handle
  };

  // Send buffer to serial output queue
  if(xQueueSend(serialGetTxQueue(), &serialWriteMessage, 100) != pdTRUE) {
    // If we couldn't send the buffer, make sure to free buff
    vPortFree(buff);
  }
}
