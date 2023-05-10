#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "bsp.h"
#include "cobs.h"
#include "crc.h"
#include "debug.h"
#include "stm32u5xx_ll_usart.h"
#include "task_priorities.h"
#include "ncp.h"
#include "ncp_uart.h"

#define NCP_NOTIFY_BUFF_MASK ( 1 << 0)
#define NCP_NOTIFY (1 << 1)


static uint32_t ncpRXBuffIdx = 0;
static uint8_t ncpRXCurrBuff = 0;
static uint8_t ncpRXBuff[2][NCP_BUFF_LEN];
static uint32_t ncpRXBuffLen[2];
static uint8_t ncpRXBuffDecoded[NCP_BUFF_LEN];

static uint8_t ncp_tx_buff[NCP_BUFF_LEN];

// static const NCPConfig_t *_config;

static TaskHandle_t ncpRXTaskHandle;

static SerialHandle_t *ncpSerialHandle = NULL;

static void ncpRXTask(void *parameters);

// Send out cobs encoded message over serial port
static bool cobs_tx(const uint8_t *buff, size_t len) {
  bool rval = false;
  configASSERT(buff);
  configASSERT(len);

  // reset the whole buff to zero
  memset(&ncp_tx_buff, 0, NCP_BUFF_LEN);

  // we then need to convert the whole thing to cobs!
  cobs_encode_result cobs_result = cobs_encode(&ncp_tx_buff, NCP_BUFF_LEN - 1, buff, len);

  if(cobs_result.status == COBS_ENCODE_OK) {
    // then we send the buffer out!
    serialWrite(ncpSerialHandle, ncp_tx_buff, cobs_result.out_len + 1);
    rval = true;
  }

  return rval;
}

void ncpInit(SerialHandle_t *ncpUartHandle){
  // here we will change the defualt rx interrupt routine to the custom one we have here
  // and then we will initialize the ncpRXTask

  // Initialize the serial handle
  configASSERT(ncpUartHandle);
  ncpSerialHandle = ncpUartHandle;

  ncpSerialHandle->txStreamBuffer = xStreamBufferCreate(ncpSerialHandle->txBufferSize, 1);
  configASSERT(ncpSerialHandle->txStreamBuffer != NULL);

  ncpSerialHandle->rxStreamBuffer = xStreamBufferCreate(ncpSerialHandle->rxBufferSize, 1);
  configASSERT(ncpSerialHandle->rxStreamBuffer != NULL);

  // Set the rxBytesFromISR to the custom NCP one
  ncpSerialHandle->rxBytesFromISR = ncpRXBytesFromISR;

  // Create the task
  BaseType_t rval = xTaskCreate(
              ncpRXTask,
              "NCP",
              configMINIMAL_STACK_SIZE * 3,
              NULL,
              NCP_TASK_PRIORITY,
              &ncpRXTaskHandle);
  configASSERT(rval == pdTRUE);

  // Set NCP tx function
  ncp_set_tx_fn(cobs_tx);

  serialEnable(ncpSerialHandle);
}

void ncpRXTask( void *parameters) {
  ( void ) parameters;

  for (;;) {
    uint32_t taskNotifyValue = 0;
    BaseType_t res = xTaskNotifyWait(pdFALSE, UINT32_MAX, &taskNotifyValue, portMAX_DELAY);

    if (res != pdTRUE) {
      printf("Error waiting for ncp task notification\n");
      continue;
    }

    // this is used to determine which static buffer to read from
    uint8_t bufferIdx = taskNotifyValue & NCP_NOTIFY_BUFF_MASK;


    // Decode the COBS
    cobs_decode_result cobs_result = cobs_decode(ncpRXBuffDecoded, NCP_BUFF_LEN, ncpRXBuff[bufferIdx], ncpRXBuffLen[bufferIdx]);
    ncp_packet_t *packet = reinterpret_cast<ncp_packet_t *>(ncpRXBuffDecoded);

    ncp_process_packet(packet, cobs_result.out_len);
  }
}

// NCP USART rx irq
// TODO - make this not USART3 dependent?
#ifndef DEBUG_USE_USART3
extern "C" void USART3_IRQHandler(void) {
    configASSERT(ncpSerialHandle);
    serialGenericUartIRQHandler(ncpSerialHandle);
}
#endif

// cppcheck-suppress constParameter
BaseType_t ncpRXBytesFromISR(SerialHandle_t *handle, uint8_t *buffer, size_t len) {
  ( void ) handle;

  // Here we will just fill up the buffers until we receive a 0x00 char and then notify the task.
  BaseType_t higherPriorityTaskWoken = pdFALSE;

  uint8_t byte = buffer[0];

  configASSERT(buffer != NULL);

  configASSERT(len == 1);

  // but the byte into the current buffer
  ncpRXBuff[ncpRXCurrBuff][ncpRXBuffIdx++] = byte;

  do {
    if(ncpRXBuffIdx >= NCP_BUFF_LEN) {
      // too much data so lets reset the current buffer
      ncpRXBuffIdx = 0;
      break;
    }

    if(byte != 0) {
      // Keep receiving data
      break;
    }

    if(ncpRXBuffIdx <= 1) {
      // Empty packet, reset current buffer
      ncpRXBuffIdx = 0;
      break;
    }

    // set the length of the current buff to the idx - 1 since we don't need the 0x00
    ncpRXBuffLen[ncpRXCurrBuff] = ncpRXBuffIdx - 1;

    ncpRXBuffIdx = 0;

    BaseType_t rval = xTaskNotifyFromISR( ncpRXTaskHandle,
                                          (ncpRXCurrBuff | NCP_NOTIFY),
                                          eSetValueWithoutOverwrite,
                                          &higherPriorityTaskWoken);
    if (rval == pdFALSE) {
      // previous packet still pending, 😬
      // TODO - track dropped packets?
      configASSERT(0);
    } else {
      // switch ncp buffers
      ncpRXCurrBuff ^= 1;
    }
  } while(0);

  return higherPriorityTaskWoken;
}
