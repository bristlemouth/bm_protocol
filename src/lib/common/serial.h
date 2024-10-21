#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "FreeRTOS.h"
#include "io.h"
#include "queue.h"
#include "stream_buffer.h"
#include "trace.h"

#ifdef __cplusplus
extern "C" {
#endif

  //
  // Maximum time to spend transmitting a single message
  //
  // At 9600 baud, that's ~6kB. At 115200 that's 72kB. If you attempt
  // to transmit a single buffer larger than that (at those baud rates)
  // some data may be lost! (with a 5 second max time)
  //
#define MAX_TX_TIME_MS (5000)

typedef struct {
  uint8_t *buffer;
  size_t idx;
  size_t len;
  void (*lineCallback)(void *serialHandle, uint8_t *line, size_t len);
} SerialLineBuffer_t;

typedef struct SerialHandle {
  // Pointer to hardware struct
  void * device;

  // Name of peripheral (for debug)
  const char *name;

  //
  // TODO - have separate struct for buffers, callbacks, etc.
  //        and use functions to register/allocate them when needed
  //

  // Pin references for low power enable/disable
  IOPinHandle_t *txPin;
  IOPinHandle_t *rxPin;
  IOPinHandle_t *interruptPin;

  // Tx stream buffer to feed ISR
  StreamBufferHandle_t txStreamBuffer;

  // Rx stream buffer to receive from ISR
  StreamBufferHandle_t rxStreamBuffer;

  // Size of Tx stream buffer
  uint32_t txBufferSize;

  // Size of Rx stream buffer
  uint32_t rxBufferSize;

  // ISR rx byte processing function
  BaseType_t (*rxBytesFromISR)(struct SerialHandle *handle, uint8_t *buffer, size_t len);

  // ISR tx byte processing function
  size_t (*getTxBytesFromISR)(struct SerialHandle *handle, uint8_t *buffer, size_t len);

  // Function to process received bytes
  void (*processByte)(void *serialHandle, uint8_t byte);

  // Pointer for additonal arguments/data to link to this handle
  void *data;

  // Interface enabled
  volatile bool enabled;

  // Misc flags
  volatile uint32_t flags;

  // Function to run before tx (called from task context)
  void (*preTxCb)(struct SerialHandle *handle);

  // Function to run after tx (called from ISR context)
  void (*postTxCb)(struct SerialHandle *handle);
} SerialHandle_t;

// Dropped rx characters
#define SERIAL_FLAG_RXDROP (1 << 0)

// Dropped tx characters
#define SERIAL_FLAG_TXDROP (1 << 1)

// TX In Progress
#define SERIAL_TX_IN_PROGRESS (1 << 2)

typedef struct {
  uint8_t *buff;
  uint16_t len;
  void *destination;
} SerialMessage_t;

// Serial handle->device's will be pointing to high memory values
// USB CDC reuses the device field for the CDC device index
#define HANDLE_IS_USB(handle) (((uint32_t)handle->device) < 0x0F)
#define HANDLE_CDC_ITF(handle) ((uint32_t)handle->device & 0x0F)

BaseType_t serialGenericRxBytesFromISR(SerialHandle_t *handle, uint8_t *buffer, size_t len);
size_t serialGenericGetTxBytes(SerialHandle_t *handle, uint8_t *buffer, size_t len);
size_t serialGenericGetTxBytesFromISR(SerialHandle_t *handle, uint8_t *buffer, size_t len);
void serialGenericUartIRQHandler(SerialHandle_t *handle);
void serialPutcharUnbuffered(SerialHandle_t *handle, char character);

void serialEnable(SerialHandle_t *handle);
void serialDisable(SerialHandle_t *handle);
void serialGenericRxTask(void *parameters);
void serialTxTask(void *parameters);
void startSerial();

void serialWrite(SerialHandle_t *handle, const uint8_t *buff, size_t len);
void serialWriteNocopy(SerialHandle_t *handle, uint8_t *buff, size_t len);

void serialSetTxLevel(SerialHandle_t *handle);
void serialResetTxLevel(SerialHandle_t *handle);

xQueueHandle serialGetTxQueue();

#ifdef TRACE_SERIAL
static inline void traceAddSerial(SerialHandle_t *handle, uint8_t byte, bool tx, bool isr) {
  uint32_t arg = 0;
  arg = (uint32_t)handle & 0xFFFF;
  if(tx) {
    // Tx Flag
    arg |= 1 << 16;
  }

  if(isr) {
    // ISR Flag
    arg |= 1 << 17;
  }
  arg |= (uint32_t)byte << 24;

  traceAdd(kTraceEventSerialByte, (void *)arg);
}

#endif

#ifdef __cplusplus
}
#endif
