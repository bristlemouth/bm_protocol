#pragma once

#include <stdint.h>
#include "FreeRTOS.h"
#include "serial.h"

#ifdef __cplusplus
extern "C" {
#endif

// Incoming uart rx buffer size (From DebugUartRxTask to CLI or other process)
#define CONSOLE_RX_BUFF_SIZE 256

// Number of bytes to allocate for single console message
#define CONSOLE_OUTPUT_SIZE 128

void startSerialConsole(SerialHandle_t *handle);
void serialConsolePutchar(char character, void* arg);
void serialConsolePutcharUnbuffered(const char chr);

size_t usbCDCRxBytesFromISR(uint8_t* buffer, size_t len);
size_t usbCDCGetTxBytes(uint8_t *buffer, size_t len);
size_t usbCDCGetTxBytesFromISR(uint8_t *buffer, size_t len);

bool serialConsoleUSBTxInProgress();

void usbCDCConnectedFromISR();
void usbCDCDisconnectedFromISR();
void serialConsoleDisable();

#ifdef __cplusplus
}
#endif
