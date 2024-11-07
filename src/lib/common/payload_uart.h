#pragma once

#include "bsp.h"
#include "serial.h"
#include "usart.h"

namespace PLUART {

#define DEFAULT_POST_TRANSACTION_WAIT_MS MAX_TX_TIME_MS

// Function pointers for hardware-specific actions for transaction control
typedef void (*HardwareControlFunc)();
void enableTransactions(HardwareControlFunc preTxFunc, HardwareControlFunc postTxFunc);
void startTransaction();
bool endTransaction(uint32_t wait_ms = DEFAULT_POST_TRANSACTION_WAIT_MS);


// Set the baud rate for the LPUART
void setBaud(uint32_t new_baud_rate);

// Set the even parity for the LPUART
void setEvenParity(void);
// Invert TX pin data
void enableDataInversion(void);
// Enable the payload UART
void enable(void);

void disable(void);

// Return true if there is a byte to read, otherwise return false
bool byteAvailable(void);

// Read a byte from the UART
uint8_t readByte(void);

// Return true if there is a line to read, otherwise return false
bool lineAvailable(void);

// Read a line from the UART - return length read
uint16_t readLine(char *buffer, size_t len);

// Write to the UART
void write(uint8_t *buffer, size_t len);

// Getter and setter for line termination character
char getTerminationCharacter();
void setTerminationCharacter(char term_char);

// Getter and setter for _useLineBuffer
bool getUseLineBuffer();
void setUseLineBuffer(bool enable);

// Getter and setter for _useByteStreamBuffer
bool getUseByteStreamBuffer();
void setUseByteStreamBuffer(bool enable);

// Clear the uart buffer and reset the uart
void reset(void);

// Setup Rx and Tx stream buffers, and start the Rx handler task LPUartRx
BaseType_t init(uint8_t task_priority);

// external declarations for the variables
extern SerialLineBuffer_t lpUART1LineBuffer;
extern SerialHandle_t uart_handle;

// Buffer length for LPUART1
#define LPUART1_LINE_BUFF_LEN 2048

// Configuring TX and TX GPIO
void setTxPinOutputLevel(void);
void resetTxPinOutputLevel(void);
void configTxPinOutput(void);
void configTxPinAlternate(void);

} // namespace PLUART
