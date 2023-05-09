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

#include "ncp_uart.h"

#define NCP_NOTIFY_BUFF_MASK ( 1 << 0)
#define NCP_NOTIFY (1 << 1)


static uint32_t ncpRXBuffIdx = 0;
static uint8_t ncpRXCurrBuff = 0;
static uint8_t ncpRXBuff[2][NCP_BUFF_LEN];
static uint32_t ncpRXBuffLen[2];
static uint8_t ncpRXBuffDecoded[NCP_BUFF_LEN];

static uint8_t ncpTXBuff[NCP_BUFF_LEN];

// static const NCPConfig_t *_config;

static TaskHandle_t ncpRXTaskHandle;

static SerialHandle_t *ncpSerialHandle = NULL;

static void ncpRXTask(void *parameters);

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

    // calc the crc16 and compare
    uint16_t crc16_post = 0;
    crc16_post = crc16_ccitt(crc16_post, reinterpret_cast<uint8_t *>((reinterpret_cast<bcmp_ncp_packet_t *>(ncpRXBuffDecoded))->type), (ncpRXBuffLen[bufferIdx] - sizeof(uint16_t)));

    if (crc16_post != reinterpret_cast<bcmp_ncp_packet_t *>(ncpRXBuffDecoded)->crc16) {
      printf("CRC16 invalid!\n");
      continue;
    }

    switch((reinterpret_cast<bcmp_ncp_packet_t*>(ncpRXBuffDecoded))->type){
      case BCMP_NCP_UNSUB: {
        // call unsub stuff here
      }

      case BCMP_NCP_SUB: {
        // call sub stuff here
      }

      case BCMP_NCP_PUB: {
        // call pub/sub stuff here
      }
      case BCMP_NCP_LOG: {
        // TODO - I guess here we can have some sort of logic to determine if this device has
        // a debug_uart, SD card, usb, or some other log output and send the data that way
      }
      case BCMP_NCP_ACK:
      case BCMP_NCP_DEBUG: {
        printf("\nDecoded message: ");
        for(uint32_t j = 0; j < (cobs_result.out_len - sizeof(bcmp_ncp_packet_t)); j++) {
          printf("%c", (reinterpret_cast<bcmp_ncp_packet_t*>(ncpRXBuffDecoded))->payload[j]);
        }
        printf("\n");
        break;
      }

      default: {
        printf("Wrong message type!\n");
        break;
      }
    }
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

  if (ncpRXBuffIdx < (NCP_BUFF_LEN)) {
    // If we have received the 0x00 byte then the COBS packet is complete
    // So we can now compile the packet information and notify the task
    if (byte == 0x00) {

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
      } else {
        // switch ncp buffers
        ncpRXCurrBuff ^= 1;
      }

    }
  } else {
    // too much data so lets reset the current buffer
    ncpRXBuffIdx = 0;
  }

  return higherPriorityTaskWoken;
}

bool ncpTx(bcmp_ncp_message_t type, uint8_t *buff, size_t len) {
  configASSERT(ncpSerialHandle);
  configASSERT(buff);

  bool rval = true;

  // Lets amke sure that what we are trying to send will fit in the buff
  configASSERT((uint32_t)len + sizeof(bcmp_ncp_packet_t) < NCP_BUFF_LEN - 1);

  bcmp_ncp_packet_t *bcmp_ncp_message;

  uint16_t bcmp_ncp_message_len = sizeof(bcmp_ncp_packet_t) + len;

  // reset the whole buff to zero
  memset(&ncpTXBuff, 0, NCP_BUFF_LEN);

  bcmp_ncp_message = static_cast<bcmp_ncp_packet_t *>(pvPortMalloc(bcmp_ncp_message_len));
  configASSERT(bcmp_ncp_message != NULL);

  bcmp_ncp_message->type = type;
  bcmp_ncp_message->flags = 0; // Unused for now
  bcmp_ncp_message->crc16 = 0;
  memcpy(bcmp_ncp_message->payload, buff, len);
  // and then we need to calculate the crc16 on the buffer only?
  bcmp_ncp_message->crc16 = crc16_ccitt(bcmp_ncp_message->crc16, reinterpret_cast<uint8_t *>(bcmp_ncp_message->type), (bcmp_ncp_message_len - sizeof(uint16_t)));

  // we then need to convert the whole thing to cobs!
  cobs_encode_result cobs_result = cobs_encode(&ncpTXBuff, NCP_BUFF_LEN - 1, bcmp_ncp_message, bcmp_ncp_message_len);

  // then we send the buffer out!
  serialWrite(ncpSerialHandle, ncpTXBuff, cobs_result.out_len + 1);

  vPortFree(bcmp_ncp_message);

  return rval;
}
