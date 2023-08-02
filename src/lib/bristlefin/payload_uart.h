#pragma once

#include "serial.h"
#include "bsp.h"
#include "usart.h"

namespace PLUART {

// Process a received byte and store it in a line buffer
  void processLineBufferedRxByte(void *serialHandle, uint8_t byte);

// User-defined line processing function
  void userProcessRxByte(const uint8_t byte) __attribute__((weak));

// Print a line of received data to USB console, Spotter Console, Spotter SD
  void processLine(void *serialHandle, uint8_t *line, size_t len);

// User-defined line processing function
  void userProcessLine(uint8_t *line, size_t len) __attribute__((weak));

// Set the baud rate for the LPUART
  void setBaud(uint32_t new_baud_rate);

  /// TODO - placeholder for RBR command UART Tx
  //  uint8_t testByte = 0xBE;
//  serialWrite(&lpuart1, &testByte, 1);

// Getter and setter for line termination character
  char getTerminationCharacter();
  void setTerminationCharacter(char term_char);

// Setup Rx and Tx stream buffers, and start the Rx handler task LPUartRx
  BaseType_t initPayloadUart(uint8_t task_priority);

// external declarations for the variables
  extern SerialLineBuffer_t lpUART1LineBuffer;
  extern SerialHandle_t uart_handle;

// Buffer length for LPUART1
#define LPUART1_LINE_BUFF_LEN 2048

}  // namespace PLUART
