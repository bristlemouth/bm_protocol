#include <stdbool.h>
#include "cli.h"
#include "debug.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "serial.h"
#include "serial_console.h"
#include "stream_buffer.h"
#include "sysMon.h"
#include "task.h"
#include "task_priorities.h"
#include "trace.h"
#include "tusb.h"
#include "util.h"

// Enable echo
const bool bEcho = true;
volatile bool txInProgress;

static void processConsoleRxByte(void *serialHandle, uint8_t byte);
SerialHandle_t usbCDC   = {
  .device = NULL,
  .name = "vcp",
  .txPin = NULL,
  .rxPin = NULL,
  .txStreamBuffer = NULL,
  .rxStreamBuffer = NULL,
  .txBufferSize = 1024,
  .rxBufferSize = 512,
  .rxBytesFromISR = NULL,
  .getTxBytesFromISR = NULL,
  .processByte = processConsoleRxByte,
  .data = NULL,
  .enabled = false,
  .flags = 0,
};

static SerialMessage_t xConsoleOutputMessage = {NULL, 0, NULL};

static uint8_t *ulConsoleRxBuff;
static uint32_t ulConsoleBuffIdx;

// Default print to usb
static SerialHandle_t *defaultPrintHandle = &usbCDC;

void startSerialConsole() {
  BaseType_t rval;

  // Single byte trigger level for fast response time
  usbCDC.txStreamBuffer = xStreamBufferCreate(usbCDC.txBufferSize, 1);
  configASSERT(usbCDC.txStreamBuffer != NULL);

  usbCDC.rxStreamBuffer = xStreamBufferCreate(usbCDC.rxBufferSize, 1);
  configASSERT(usbCDC.rxStreamBuffer != NULL);

  // Buffer to hold data before sending to CLI
  ulConsoleRxBuff = (uint8_t *)pvPortMalloc(CONSOLE_RX_BUFF_SIZE);
  configASSERT(ulConsoleRxBuff != NULL);

  xConsoleOutputMessage.buff = (uint8_t *)pvPortMalloc(CONSOLE_OUTPUT_SIZE);
  configASSERT(xConsoleOutputMessage.buff != NULL);
  xConsoleOutputMessage.len = 0;

  debugInit(serialConsolePutchar);

  rval = xTaskCreate(
              serialGenericRxTask,
              "USBCDCRx",
              // TODO - verify stack size
              configMINIMAL_STACK_SIZE * 2,
              &usbCDC,
              USBCDC_RX_TASK_PRIORITY,
              NULL);
  configASSERT(rval == pdTRUE);

}

void serialConsolePutcharUnbuffered(const char chr) {
  serialPutcharUnbuffered(defaultPrintHandle, chr);
}

void serialConsolePutchar(char character, void* arg) {
  (void) arg;
  // Only print if the interface is connected
  if(defaultPrintHandle->enabled) {
    xConsoleOutputMessage.buff[xConsoleOutputMessage.len++] = (uint8_t)character;

    // If message buffer is full (or we see a newline), send buffer
    if (xConsoleOutputMessage.len >= CONSOLE_OUTPUT_SIZE || character == '\n') {
      xConsoleOutputMessage.destination = defaultPrintHandle;
      if(xQueueSend(serialGetTxQueue(), &xConsoleOutputMessage, 10) != pdTRUE) {
        // Free buffer if unable to send
        vPortFree(xConsoleOutputMessage.buff);
      }

      // Allocate console buffer
      xConsoleOutputMessage.buff = (uint8_t *)pvPortMalloc(CONSOLE_OUTPUT_SIZE);
      configASSERT(xConsoleOutputMessage.buff != NULL);
      xConsoleOutputMessage.len = 0;
    }
  }
  // TODO - do other things with the data if interface is not enabled
  // perhaps save to log file
}

static void processConsoleRxByte(void *serialHandle, uint8_t byte) {
  configASSERT(serialHandle != NULL);
  SerialHandle_t *handle = (SerialHandle_t *)serialHandle;

  ulConsoleRxBuff[ulConsoleBuffIdx] = byte;

  if(bEcho) {
    serialConsolePutcharUnbuffered((char)ulConsoleRxBuff[ulConsoleBuffIdx]);

    // Erase last character with backspace
    if((char)ulConsoleRxBuff[ulConsoleBuffIdx] == '\b') {
      serialConsolePutcharUnbuffered(' ');
      serialConsolePutcharUnbuffered('\b');
    }
  }

  if(ulConsoleRxBuff[ulConsoleBuffIdx] == '\n'){

    // Deal with the carriage return, if any
    if(ulConsoleBuffIdx > 0 && ulConsoleRxBuff[ulConsoleBuffIdx - 1] == '\r') {
      ulConsoleRxBuff[ulConsoleBuffIdx - 1] = 0;
    } else {
      ulConsoleRxBuff[ulConsoleBuffIdx] = 0;
    }

    // Set up command buffer structure
    CLICommand_t cliCmd = {
      .buff = ulConsoleRxBuff,
      .len = (uint16_t)ulConsoleBuffIdx,
      .src = handle
    };

    // Send line to CLI processor
    BaseType_t rval = xQueueSend(cliGetQueue(), &cliCmd, 0U );
    if(rval == errQUEUE_FULL) {
      // CLI queue was full, lets reuse the allocated buffer
      // Could print out an error message instead?
    } else {
      // Buffer was sent, it is up to the CLI task to free
      // Allocate next rx buffer
      ulConsoleRxBuff = (uint8_t *)pvPortMalloc(CONSOLE_RX_BUFF_SIZE);
      configASSERT(ulConsoleRxBuff != NULL);
    }

    // Reset buffer index
    ulConsoleBuffIdx = 0;
  } else {
    // Handle backspaces
    if((char)ulConsoleRxBuff[ulConsoleBuffIdx] == '\b') {
      if(ulConsoleBuffIdx > 0) {
        ulConsoleBuffIdx--;
      } // ignore backspace otherwise
    } else {
      ulConsoleBuffIdx++;
    }

    // Not enough room in input buffer. Purge data
    if(ulConsoleBuffIdx >= CONSOLE_RX_BUFF_SIZE) {
      printf("\nRX overflow - clearing buffer!\n");
      ulConsoleBuffIdx = 0;
    }

  }
}

/*!
  Enable serial device for usbCDC and reset all buffers. Also,
  call usbConnected() in case we have to do anything when console
  is opened.
*/
void serialConsoleEnable() {
  usbCDC.enabled = true;
  ulConsoleBuffIdx = 0;
  txInProgress = false;
  xStreamBufferReset(usbCDC.rxStreamBuffer);
  xStreamBufferReset(usbCDC.txStreamBuffer);
  usbConnected();
}

/*!
  Disable usbCDC serial device
*/
void serialConsoleDisable() {
  usbCDC.enabled = false;
}

/*!
  Callback when CDC line state changes
  Invoked when cdc when line state changed e.g connected/disconnected

  \param[in] itf - unused
  \param[in] dtr - dtr signal
  \param[in] rts - rts signal

*/
void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts)
{
  (void) itf;
  (void) rts;

  if ( dtr ) {
    // Terminal connected
    serialConsoleEnable();
  } else {
    // Terminal disconnected
    serialConsoleDisable();
  }
}

// Invoked when usb bus is suspended
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en) {
  (void)remote_wakeup_en;
  serialConsoleDisable();
}


static uint8_t usbRxBuff[2048];
/*!
  Receive data over USB and send to rxStreamBuffer

  \param[in] itf - unused
*/
void tud_cdc_rx_cb(uint8_t itf) {
  (void)itf;

  // TODO - skip the streambuffer alltogether since we're
  // double buffering and there's no longer interrupts
  // to worry about with tinyUSB.
  // Leaving as-is for now to just get it working...

  // Get bytes from USB
  uint32_t count = tud_cdc_read(usbRxBuff, sizeof(usbRxBuff));

  if(count) {
#ifdef TRACE_SERIAL
    for(uint32_t byte=0; byte < len; byte++) {
      traceAddSerial(&usbCDC, buffer[byte], false, true);
    }
#endif

    // Add bytes to streambuffer
    size_t bytesSent = xStreamBufferSend(
                          usbCDC.rxStreamBuffer,
                          usbRxBuff,
                          count,
                          100);

    if(bytesSent != count) {
      // Flag that some bytes were dropped
      usbCDC.flags |= SERIAL_FLAG_RXDROP;

    }
  }
}

/*!
  Check if console USB tx is in progress

  \return true if we're currently sending data over USB
*/
bool serialConsoleUSBTxInProgress() {
  return txInProgress;
}

static uint8_t usbTxBuff[64];
/*!
  Callback when USB tx is complete. Sends more data if available, does
  nothing else otherwise.

  \param[in] itf - unused
*/
void tud_cdc_tx_complete_cb(uint8_t itf) {
  (void)itf;

  do {
    uint32_t bytesToSend = tud_cdc_write_available();
    if(!bytesToSend) {
      break;
    }

    bytesToSend = MIN(bytesToSend, sizeof(usbTxBuff));
    bytesToSend = serialGenericGetTxBytes(&usbCDC, usbTxBuff, bytesToSend);

    if(!bytesToSend) {
      // Done transmitting
      txInProgress = false;
      break;
    } else {
      txInProgress = true;
    }

    volatile uint32_t bytesSent = tud_cdc_write(usbTxBuff, bytesToSend);
    configASSERT(bytesSent == bytesToSend);

    tud_cdc_write_flush();

  } while(0);
}
