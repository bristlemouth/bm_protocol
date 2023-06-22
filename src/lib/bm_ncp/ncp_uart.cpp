#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "bm_pubsub.h"
#include "bm_serial.h"
#include "bsp.h"
#include "cobs.h"
#include "crc.h"
#include "debug.h"
#include "device_info.h"
#include "ncp_uart.h"
#include "stm32_rtc.h"
#include "stm32u5xx_ll_usart.h"
#include "task_priorities.h"
#include "ncp_dfu.h"
#include "ncp_config.h"

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
static BaseType_t ncpRXBytesFromISR(SerialHandle_t *handle, uint8_t *buffer, size_t len);

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

static bm_serial_callbacks_t bm_serial_callbacks;

static void ncp_uart_pub_cb(uint64_t node_id, const char* topic, uint16_t topic_len, const uint8_t* data, uint16_t data_len, uint8_t type, uint8_t version) {
  bm_serial_pub(node_id, topic, topic_len, data, data_len, type, version);
}

static bool bm_serial_pub_cb(const char *topic, uint16_t topic_len, uint64_t node_id, const uint8_t *payload, size_t len, uint8_t type, uint8_t version) {
  printf("Pub data on topic \"%.*s\" from %" PRIx64 " Type: %u, Version: %u\n", topic_len, topic, node_id, type, version);
  (void)payload;
  (void)len;

  //
  // Ignoring published data from spotter right now
  //
  return false;
}

static bool bm_serial_sub_cb(const char *topic, uint16_t topic_len) {
  return bm_sub_wl(topic, topic_len, ncp_uart_pub_cb);
}

static bool bm_serial_unsub_cb(const char *topic, uint16_t topic_len) {
  return bm_unsub_wl(topic, topic_len, ncp_uart_pub_cb);
}

static bool ncp_log_cb(uint64_t node_id, const uint8_t *data, size_t len) {
  printf("NCP Log from %" PRIx64 ": %.*s\n", node_id, (int)len, data);

  return false;
}

static bool ncp_debug_cb(const uint8_t *data, size_t len) {
  printf("NCP debug: %.*s\n", (int)len, data);

  return false;
}

static bool bm_serial_rtc_cb(bm_serial_time_t *time) {
  RTCTimeAndDate_t rtc_time = {
    .year = time->year,
    .month = time->month,
    .day = time->day,
    .hour = time->hour,
    .minute = time->minute,
    .second = time->second,
    .ms = (time->us / 1000),
  };

  printf("Updating RTC to %u-%u-%u %02u:%02u:%02u.%04u\n",
    rtc_time.year,
    rtc_time.month,
    rtc_time.day,
    rtc_time.hour,
    rtc_time.minute,
    rtc_time.second,
    rtc_time.ms);

  // NOTE: rtcSet ignores milliseconds right now
  return (rtcSet(&rtc_time) == pdPASS);
}

// Used by spotter to request a self test
static bool bm_serial_self_test_cb(uint64_t node_id, uint32_t result) {
  (void)node_id;
  (void)result;
  printf("Running self test\n");
  // TODO - actually run some sort of test 🤷‍♂️

  // Send back a "PASS"
  return (bm_serial_send_self_test(getNodeId(), 1) == BM_SERIAL_OK);
}

void ncpInit(SerialHandle_t *ncpUartHandle, NvmPartition *dfu_partition, BridgePowerController *power_controller,
  cfg::Configuration* usr_cfg, cfg::Configuration* sys_cfg, cfg::Configuration* hw_cfg){
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

  configASSERT(dfu_partition);
  configASSERT(power_controller);
  ncp_dfu_init(dfu_partition, power_controller);
  ncp_cfg_init(usr_cfg, sys_cfg, hw_cfg);

  // Create the task
  BaseType_t rval = xTaskCreate(
              ncpRXTask,
              "NCP",
              configMINIMAL_STACK_SIZE * 3,
              NULL,
              NCP_TASK_PRIORITY,
              &ncpRXTaskHandle);
  configASSERT(rval == pdTRUE);

  bm_serial_callbacks.tx_fn = cobs_tx;
  bm_serial_callbacks.pub_fn = bm_serial_pub_cb;
  bm_serial_callbacks.sub_fn = bm_serial_sub_cb;
  bm_serial_callbacks.unsub_fn = bm_serial_unsub_cb;
  bm_serial_callbacks.log_fn = ncp_log_cb;
  bm_serial_callbacks.debug_fn = ncp_debug_cb;
  bm_serial_callbacks.rtc_set_fn = bm_serial_rtc_cb;
  bm_serial_callbacks.self_test_fn = bm_serial_self_test_cb;
  bm_serial_callbacks.dfu_start_fn = ncp_dfu_start_cb;
  bm_serial_callbacks.dfu_chunk_fn = ncp_dfu_chunk_cb;
  bm_serial_callbacks.dfu_end_fn = NULL;
  bm_serial_callbacks.cfg_get_fn = ncp_cfg_get_cb;
  bm_serial_callbacks.cfg_set_fn = ncp_cfg_set_cb;
  bm_serial_callbacks.cfg_value_fn = NULL;
  bm_serial_callbacks.cfg_commit_fn = ncp_cfg_commit_cb;
  bm_serial_callbacks.cfg_status_request_fn = ncp_cfg_status_request_cb;
  bm_serial_callbacks.cfg_status_response_fn = NULL;
  bm_serial_callbacks.cfg_key_del_request_fn = ncp_cfg_key_del_request_cb;
  bm_serial_callbacks.cfg_key_del_response_fn = NULL;
  bm_serial_set_callbacks(&bm_serial_callbacks);

  serialEnable(ncpSerialHandle);
  ncp_dfu_check_for_update();
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
    bm_serial_packet_t *packet = reinterpret_cast<bm_serial_packet_t *>(ncpRXBuffDecoded);

    bm_serial_process_packet(packet, cobs_result.out_len);
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
static BaseType_t ncpRXBytesFromISR(SerialHandle_t *handle, uint8_t *buffer, size_t len) {
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

    if(ncpRXBuffIdx <= sizeof(bm_serial_packet_t)) {
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
