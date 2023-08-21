#pragma once

#include "bsp.h"
#include "usart.h"
#include "serial.h"

namespace PLUART {

  // Set the baud rate for the LPUART
  void setBaud(uint32_t new_baud_rate);

  // Enable the payload UART
  void enable(void);

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

  // Setup Rx and Tx stream buffers, and start the Rx handler task LPUartRx
  BaseType_t init(uint8_t task_priority);

  // external declarations for the variables
  extern SerialLineBuffer_t lpUART1LineBuffer;
  extern SerialHandle_t uart_handle;

// Buffer length for LPUART1
#define LPUART1_LINE_BUFF_LEN 2048

}  // namespace PLUART
