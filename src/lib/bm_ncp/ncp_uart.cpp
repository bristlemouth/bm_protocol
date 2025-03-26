#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include <string.h>

#include "app_util.h"
#include "bcmp.h"
#include "bm_serial.h"
#include "bsp.h"
#include "cobs.h"
#include "crc.h"
#include "debug.h"
#include "device_info.h"
#include "gpio.h"
#include "memfault/core/reboot_tracking.h"
#include "memfault_platform_core.h"
extern "C" {
#include "messages/info.h"
#include "messages/resource_discovery.h"
#include "topology.h"
}
#include "ncp_config.h"
#include "ncp_dfu.h"
#include "ncp_uart.h"
#include "pubsub.h"
#include "reset_reason.h"
#include "stm32_rtc.h"
#include "stm32u5xx_ll_usart.h"
#include "task_priorities.h"

#define NCP_NOTIFY_BUFF_MASK (1 << 0)
#define NCP_NOTIFY (1 << 1)
#define NCP_PROCESSOR_RX_QUEUE_DEPTH (16)
#define NCP_PROCESSOR_TX_QUEUE_DEPTH (16)
#define NCP_BAUD_RATE_NEGOTIATE_TIMEOUT_MS (10)

static uint32_t ncpRXBuffIdx = 0;
static uint8_t ncpRXCurrBuff = 0;
static uint8_t ncpRXBuff[2][NCP_BUFF_LEN];
static uint32_t ncpRXBuffLen[2];
static uint8_t ncpRXBuffDecoded[NCP_BUFF_LEN];
static volatile bool ncp_rx = false;

static QueueHandle_t ncp_processor_queue_handle;
static QueueHandle_t ncp_tx_queue_handle;
static SemaphoreHandle_t ncp_serial_lock;
static SemaphoreHandle_t ncp_baud_rate_discovery_lock = NULL;

// static const NCPConfig_t *_config;

static TaskHandle_t ncpRXTaskHandle;

static SerialHandle_t *ncpSerialHandle = NULL;

static void ncpTxTask(void *parameters);
static void ncpRXTask(void *parameters);
static void ncpRXProcessor(void *parameters);
static void ncpBaudRateDiscovery(void);
static BaseType_t ncpRXBytesFromISR(SerialHandle_t *handle, uint8_t *buffer, size_t len);
static void ncpPreTxCb(SerialHandle_t *handle);
static void ncpPostTxCb(SerialHandle_t *handle);

typedef enum {
  NCP_NEGOTIATING_NONE,
  BAUD_NEGOTIATING_REQUEST,
  BAUD_NEGOTIATING_REPLY,
} NCPNegotiatingEvent_t;

typedef struct ProcessorQueueItem {
  uint8_t *buffer;
  size_t len;
  bool negotiate_event;
} ProcessorQueueItem_t;

typedef struct {
  uint32_t requested_baud;
  NCPNegotiatingEvent_t event;
  TimerHandle_t negotiate_timer;
  SemaphoreHandle_t negotiating_lock;
  bool baud_found;
} NCPNegotiatingContext_t;
static NCPNegotiatingContext_t ncp_ctx = {};

static const uint32_t acceptable_bauds[] = {
    57600,
    115200,
    1000000,
};

// Send out cobs encoded message over serial port
static bool queue_tx(const uint8_t *buff, size_t len, bm_serial_message_t message) {
  bool rval = false;
  configASSERT(buff);
  configASSERT(len);

  uint8_t *tmp_buf = (uint8_t *)pvPortMalloc(len);
  ProcessorQueueItem_t q_msg = {
      .buffer = tmp_buf,
      .len = len,
      .negotiate_event = false,
  };

  if (message == BM_SERIAL_BAUD_RATE_REQ || message == BM_SERIAL_BAUD_RATE_REPLY) {
    q_msg.negotiate_event = true;
  }

  if (tmp_buf) {
    memcpy(tmp_buf, buff, len);
    if (xQueueSend(ncp_tx_queue_handle, &q_msg, pdMS_TO_TICKS(10)) != pdTRUE) {
      vPortFree(tmp_buf);
      printf("Could not queue buffer for NCP message transmission");
    } else {
      rval = true;
    }
  } else {
    printf("Could not allocate memory for NCP message transmission");
  }

  return rval;
}

static bm_serial_callbacks_t bm_serial_callbacks;

static void ncp_uart_pub_cb(uint64_t node_id, const char *topic, uint16_t topic_len,
                            const uint8_t *data, uint16_t data_len, uint8_t type,
                            uint8_t version) {
  printf("Publishing To Topic: %s\n", topic);
  bm_serial_pub(node_id, topic, topic_len, data, data_len, type, version);
}

static bool bm_serial_pub_cb(const char *topic, uint16_t topic_len, uint64_t node_id,
                             const uint8_t *payload, size_t len, uint8_t type,
                             uint8_t version) {
  printf("Pub data on topic \"%.*s\" from %016" PRIx64 " Type: %u, Version: %u\n", topic_len,
         topic, node_id, type, version);
  return bm_pub_wl(topic, topic_len, payload, len, type, version) == BmOK;
}

static bool bm_serial_sub_cb(const char *topic, uint16_t topic_len) {
  return bm_sub_wl(topic, topic_len, ncp_uart_pub_cb) == BmOK;
}

static bool bm_serial_unsub_cb(const char *topic, uint16_t topic_len) {
  return bm_unsub_wl(topic, topic_len, ncp_uart_pub_cb) == BmOK;
}

static bool ncp_log_cb(uint64_t node_id, const uint8_t *data, size_t len) {
  printf("NCP Log from %016" PRIx64 ": %.*s\n", node_id, (int)len, data);

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

  printf("Updating RTC to %u-%u-%u %02u:%02u:%02u.%04u\n", rtc_time.year, rtc_time.month,
         rtc_time.day, rtc_time.hour, rtc_time.minute, rtc_time.second, rtc_time.ms);

  // NOTE: rtcSet ignores milliseconds right now
  return (rtcSet(&rtc_time) == pdPASS);
}

// TODO - redefine, or define in a spot where this is only needed once!
#define VER_STR_MAX_LEN 255

static void bcmp_info_reply_cb(void *bcmp_info_reply) {
  bm_serial_device_info_reply_t *info_reply =
      reinterpret_cast<bm_serial_device_info_reply_t *>(bcmp_info_reply);
  bm_serial_send_info_reply(info_reply->info.node_id, info_reply);
}

static void bcmp_resource_table_reply_cb(void *bcmp_resource_table_reply) {
  bm_serial_resource_table_reply_t *resource_reply =
      reinterpret_cast<bm_serial_resource_table_reply_t *>(bcmp_resource_table_reply);
  bm_serial_send_resource_reply(resource_reply->node_id, resource_reply);
}

static void bm_serial_all_info_cb(NetworkTopology *networkTopology) {
  configASSERT(networkTopology);
  uint16_t num_nodes = networkTopology->length;
  NeighborTableEntry *cursor = NULL;
  uint16_t counter;
  for (cursor = networkTopology->front, counter = 0; (cursor != NULL) && (counter < num_nodes);
       cursor = cursor->nextNode, counter++) {
    uint64_t current_node_id = cursor->neighbor_table_reply->node_id;
    if (current_node_id != getNodeId()) {
      bcmp_request_info(current_node_id, &multicast_global_addr, bcmp_info_reply_cb);
    }
  }
}

static void bm_serial_all_resources_cb(NetworkTopology *networkTopology) {
  configASSERT(networkTopology);
  uint16_t num_nodes = networkTopology->length;
  NeighborTableEntry *cursor = NULL;
  uint16_t counter;
  for (cursor = networkTopology->front, counter = 0; (cursor != NULL) && (counter < num_nodes);
       cursor = cursor->nextNode, counter++) {
    uint64_t current_node_id = cursor->neighbor_table_reply->node_id;
    if (current_node_id != getNodeId()) {
      bcmp_resource_discovery_send_request(current_node_id, bcmp_resource_table_reply_cb);
    }
  }
}

static bool bcmp_info_request_cb(uint64_t node_id) {

  if (node_id == getNodeId() || node_id == 0) {
    // send back our info!
    char *ver_str = static_cast<char *>(pvPortMalloc(VER_STR_MAX_LEN));
    configASSERT(ver_str);
    memset(ver_str, 0, VER_STR_MAX_LEN);

    uint8_t ver_str_len =
        snprintf(ver_str, VER_STR_MAX_LEN, "%s@%s", APP_NAME, getFWVersionStr());

    // TODO - use device name instead of UID str
    uint8_t dev_name_len = strlen(getUIDStr());

    uint16_t info_len = sizeof(bm_serial_device_info_reply_t) + ver_str_len + dev_name_len;

    uint8_t *dev_info_buff = static_cast<uint8_t *>(pvPortMalloc(info_len));
    configASSERT(info_len);

    memset(dev_info_buff, 0, info_len);

    bm_serial_device_info_reply_t *dev_info =
        reinterpret_cast<bm_serial_device_info_reply_t *>(dev_info_buff);
    dev_info->info.node_id = getNodeId();

    // TODO - fill these with actual values
    dev_info->info.vendor_id = 0;
    dev_info->info.product_id = 0;
    memset(dev_info->info.serial_num, '0', sizeof(dev_info->info.serial_num));

    dev_info->info.git_sha = getGitSHA();
    getFWVersion(&dev_info->info.ver_major, &dev_info->info.ver_minor, &dev_info->info.ver_rev);

    // TODO - get actual hardware version
    dev_info->info.ver_hw = getHwVersion();

    dev_info->ver_str_len = ver_str_len;
    dev_info->dev_name_len = dev_name_len;

    memcpy(&dev_info->strings[0], ver_str, ver_str_len);
    memcpy(&dev_info->strings[ver_str_len], getUIDStr(), dev_name_len);
    bm_serial_send_info_reply(getNodeId(), dev_info);
    vPortFree(ver_str);
    vPortFree(dev_info_buff);
    if (node_id == 0) {
      // send out a broadcast
      bcmp_topology_start(bm_serial_all_info_cb);
    }
  } else {
    // send back the info for the node_id
    bcmp_request_info(node_id, &multicast_global_addr, bcmp_info_reply_cb);
  }
  return true;
}

static bool bcmp_resource_request_cb(uint64_t node_id) {
  if (node_id == getNodeId() || node_id == 0) {
    // send back our resource info!
    BcmpResourceTableReply *local_resources = bcmp_resource_discovery_get_local_resources();
    // Sending via serial will make a copy of the resources so we can free them after sending
    bm_serial_send_resource_reply(
        getNodeId(), reinterpret_cast<bm_serial_resource_table_reply_t *>(local_resources));
    // getting the local resources allocates them, we need to free them after sending them out
    vPortFree(local_resources);
    if (node_id == 0) {
      bcmp_topology_start(bm_serial_all_resources_cb);
    }
  } else {
    // send back the resource info for the node_id
    bcmp_resource_discovery_send_request(node_id, bcmp_resource_table_reply_cb);
  }
  return true;
}

static bool node_id_request_cb(void) {
  bm_serial_send_node_id_reply(getNodeId());
  return true;
}

static void baud_rate_negotiate_timer_handle(TimerHandle_t timer) {
  (void)timer;
  xSemaphoreGive(ncp_ctx.negotiating_lock);
  if (ncp_baud_rate_discovery_lock) {
    xSemaphoreGive(ncp_baud_rate_discovery_lock);
  }
}

static bool baud_is_acceptable(uint32_t baud) {
  bool ret = false;

  for (uint8_t i = 0; i < array_size(acceptable_bauds); i++) {
    if (baud == acceptable_bauds[i]) {
      ret = true;
      break;
    }
  }

  return ret;
}

static bool baud_rate_request_cb(uint32_t baud) {
  bool ret = false;

  if (baud_is_acceptable(baud)) {
    ret = true;
    ncp_ctx.requested_baud = baud;
    ncp_ctx.event = BAUD_NEGOTIATING_REPLY;
    bm_serial_send_baud_rate_reply();
  }

  return ret;
}

static bool baud_rate_reply_cb(void) {
  xTimerReset(ncp_ctx.negotiate_timer, 0);
  xTimerStop(ncp_ctx.negotiate_timer, 0);
  serialSetBaudRate(ncpSerialHandle, ncp_ctx.requested_baud);
  if (ncp_baud_rate_discovery_lock) {
    ncp_ctx.baud_found = true;
    xSemaphoreGive(ncp_baud_rate_discovery_lock);
  }
  xSemaphoreGive(ncp_ctx.negotiating_lock);

  return true;
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

static bool bm_int_gpio_callback_fromISR(const void *pinHandle, uint8_t value, void *args) {
  (void)args;
  (void)pinHandle;

  BaseType_t xHigherPriorityTaskWoken = pdFALSE;

  if (value) {
    lpmPeripheralInactiveFromISR(LPM_USART3_RX);
    xSemaphoreGiveFromISR(ncp_serial_lock, &xHigherPriorityTaskWoken);
    ncp_rx = false;
  } else {
    xSemaphoreTakeFromISR(ncp_serial_lock, &xHigherPriorityTaskWoken);
    ncpRXBuffIdx = 0;
    lpmPeripheralActiveFromISR(LPM_USART3_RX); // Active low
    ncp_rx = true;
  }

  return xHigherPriorityTaskWoken;
}

void ncpInit(SerialHandle_t *ncpUartHandle, NvmPartition *dfu_partition,
             BridgePowerController *power_controller) {
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
  ncpSerialHandle->interruptPin = &BM_INT;

  ncpSerialHandle->preTxCb = ncpPreTxCb;
  ncpSerialHandle->postTxCb = ncpPostTxCb;

  configASSERT(dfu_partition);
  configASSERT(power_controller);
  ncp_dfu_init(dfu_partition, power_controller);
  ncp_cfg_init();
  ncp_processor_queue_handle =
      xQueueCreate(NCP_PROCESSOR_RX_QUEUE_DEPTH, sizeof(ProcessorQueueItem_t));
  configASSERT(ncp_processor_queue_handle);

  ncp_tx_queue_handle =
      xQueueCreate(NCP_PROCESSOR_TX_QUEUE_DEPTH, sizeof(ProcessorQueueItem_t));
  configASSERT(ncp_tx_queue_handle);

  ncp_serial_lock = xSemaphoreCreateBinary();
  configASSERT(ncp_serial_lock);

  ncp_ctx.negotiating_lock = xSemaphoreCreateBinary();
  configASSERT(ncp_ctx.negotiating_lock);
  xSemaphoreGive(ncp_ctx.negotiating_lock);

  ncp_ctx.negotiate_timer =
      xTimerCreate("Baud Neg Timer", pdMS_TO_TICKS(NCP_BAUD_RATE_NEGOTIATE_TIMEOUT_MS), pdFALSE,
                   NULL, baud_rate_negotiate_timer_handle);
  configASSERT(ncp_ctx.negotiate_timer);

  // Create the task
  BaseType_t rval = xTaskCreate(ncpTxTask, "NCP_TX", configMINIMAL_STACK_SIZE * 3, NULL,
                                NCP_TASK_PRIORITY, &ncpRXTaskHandle);
  configASSERT(rval == pdTRUE);

  rval = xTaskCreate(ncpRXTask, "NCP_RX", configMINIMAL_STACK_SIZE * 3, NULL, NCP_TASK_PRIORITY,
                     &ncpRXTaskHandle);
  configASSERT(rval == pdTRUE);

  rval = xTaskCreate(ncpRXProcessor, "NCP_Processor", configMINIMAL_STACK_SIZE * 3, NULL,
                     NCP_PROCESSOR_TASK_PRIORITY, NULL);
  configASSERT(rval == pdTRUE);

  bm_serial_callbacks.tx_fn = queue_tx;
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
  bm_serial_callbacks.reboot_info_fn = NULL;
  bm_serial_callbacks.network_info_fn = NULL;
  bm_serial_callbacks.bcmp_info_request_fn = bcmp_info_request_cb;
  bm_serial_callbacks.bcmp_info_response_fn = NULL;
  bm_serial_callbacks.bcmp_resource_request_fn = bcmp_resource_request_cb;
  bm_serial_callbacks.bcmp_resource_response_fn = NULL;
  bm_serial_callbacks.node_id_request_fn = node_id_request_cb;
  bm_serial_callbacks.node_id_reply_fn = NULL;
  bm_serial_callbacks.baud_rate_negotiation_request_fn = baud_rate_request_cb;
  bm_serial_callbacks.baud_rate_negotiation_reply_fn = baud_rate_reply_cb;
  bm_serial_set_callbacks(&bm_serial_callbacks);
  IORegisterCallback(&BM_INT, bm_int_gpio_callback_fromISR, NULL);

  serialEnable(ncpSerialHandle);
  ncpBaudRateDiscovery();
  ncp_dfu_check_for_update();
  bm_serial_send_reboot_info(getNodeId(), checkResetReason(), getGitSHA(),
                             memfault_reboot_tracking_get_crash_count(), memfault_get_pc(),
                             memfault_get_lr());
}

bool ncp_negotiate_baud_rate(uint32_t baud) {
  bool ret = false;

  if (xTimerIsTimerActive(ncp_ctx.negotiate_timer) != pdTRUE &&
      ncp_ctx.event == NCP_NEGOTIATING_NONE) {
    ret = true;
    ncp_ctx.requested_baud = baud;
    ncp_ctx.event = BAUD_NEGOTIATING_REQUEST;
    bm_serial_send_baud_rate_request(baud);
  }

  return ret;
}

void ncpTxTask(void *parameters) {
  (void)parameters;
  ProcessorQueueItem_t q_item;
  BaseType_t res;
  NCPNegotiatingEvent_t *negotiate_event = NULL;
  static uint8_t ncp_tx_buff[NCP_BUFF_LEN];

  while (1) {
    res = xQueueReceive(ncp_tx_queue_handle, &q_item, portMAX_DELAY);
    negotiate_event = NULL;

    if (res == pdPASS) {
      if (xSemaphoreTake(ncp_ctx.negotiating_lock, portMAX_DELAY) == pdPASS) {
        // reset the whole buff to zero
        memset(&ncp_tx_buff, 0, NCP_BUFF_LEN);

        if (q_item.negotiate_event) {
          negotiate_event = &ncp_ctx.event;
        }

        // we then need to convert the whole thing to cobs!
        cobs_encode_result cobs_result =
            cobs_encode(&ncp_tx_buff, NCP_BUFF_LEN - 1, q_item.buffer, q_item.len);

        if (cobs_result.status == COBS_ENCODE_OK) {
          serialGenericTx(ncpSerialHandle, ncp_tx_buff, cobs_result.out_len + 1,
                          negotiate_event);
        }
      } else {
        printf("Could not take serial lock to transmit message");
      }
    }
    vPortFree(q_item.buffer);
  }
}

void ncpRXTask(void *parameters) {
  (void)parameters;

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
    cobs_decode_result cobs_result = cobs_decode(ncpRXBuffDecoded, NCP_BUFF_LEN,
                                                 ncpRXBuff[bufferIdx], ncpRXBuffLen[bufferIdx]);
    // Allocate a buffer for the ncpRXProcessor to process the data.
    // Note that ncpRXProcessor will be in charge of freeing that data.
    uint8_t *processor_buffer = static_cast<uint8_t *>(pvPortMalloc(cobs_result.out_len));
    configASSERT(processor_buffer);
    memcpy(processor_buffer, ncpRXBuffDecoded, cobs_result.out_len);
    ProcessorQueueItem_t q_msg = {
        .buffer = processor_buffer,
        .len = cobs_result.out_len,
        .negotiate_event = false,
    };
    configASSERT(xQueueSend(ncp_processor_queue_handle, &q_msg, pdMS_TO_TICKS(10)) == pdTRUE);
  }
}

static void ncpRXProcessor(void *parameters) {
  (void)parameters;
  ProcessorQueueItem_t q_item;
  for (;;) {
    configASSERT(xQueueReceive(ncp_processor_queue_handle, &q_item, portMAX_DELAY) == pdPASS);
    bm_serial_packet_t *packet = reinterpret_cast<bm_serial_packet_t *>(q_item.buffer);
    bm_serial_process_packet(packet, q_item.len);
    vPortFree(packet);
  }
}

static void ncpBaudRateDiscovery(void) {
  uint8_t baud_idx = array_size(acceptable_bauds);
  static const uint8_t null_buf = 0;

  ncp_baud_rate_discovery_lock = xSemaphoreCreateBinary();
  configASSERT(ncp_baud_rate_discovery_lock);

  while (!ncp_ctx.baud_found && baud_idx > 0) {
    baud_idx--;
    // Reset buffers just incase anything was sent at a different baud rate
    ncpRXCurrBuff = 0;
    ncpRXBuffIdx = 0;
    serialSetBaudRate(ncpSerialHandle, acceptable_bauds[baud_idx]);
    bm_serial_tx(BM_SERIAL_ACK, &null_buf, sizeof(uint8_t));
    ncp_negotiate_baud_rate(1000000);
    if (xSemaphoreTake(ncp_baud_rate_discovery_lock, portMAX_DELAY) != pdTRUE) {
      printf("Could not take baud rate discovery semaphore\n");
      break;
    }
  }

  if (!ncp_ctx.baud_found) {
    serialSetBaudRate(ncpSerialHandle, 115200);
    // Send 0 to reset receiving buffer on spotter and reset our buffers just
    // incase anything was sent at a different baud rate
    ncpRXCurrBuff = 0;
    ncpRXBuffIdx = 0;
    bm_serial_tx(BM_SERIAL_ACK, &null_buf, sizeof(uint8_t));
    printf("Could not find a baudrate to communicate to spotter with\n");
  }

  // Prevent race condition with semaphore usage in other tasks
  SemaphoreHandle_t tmp_sem = ncp_baud_rate_discovery_lock;
  ncp_baud_rate_discovery_lock = NULL;
  vSemaphoreDelete(tmp_sem);
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
  (void)handle;

  // Here we will just fill up the buffers until we receive a 0x00 char and then notify the task.
  BaseType_t higherPriorityTaskWoken = pdFALSE;

  uint8_t byte = buffer[0];

  configASSERT(buffer != NULL);

  configASSERT(len == 1);

  // but the byte into the current buffer
  ncpRXBuff[ncpRXCurrBuff][ncpRXBuffIdx++] = byte;

  do {
    if (ncpRXBuffIdx >= NCP_BUFF_LEN) {
      // too much data so lets reset the current buffer
      ncpRXBuffIdx = 0;
      break;
    }

    if (byte != 0) {
      // Keep receiving data
      break;
    }

    if (ncpRXBuffIdx <= sizeof(bm_serial_packet_t)) {
      // Empty packet, reset current buffer
      ncpRXBuffIdx = 0;
      break;
    }

    // set the length of the current buff to the idx - 1 since we don't need the 0x00
    ncpRXBuffLen[ncpRXCurrBuff] = ncpRXBuffIdx - 1;

    ncpRXBuffIdx = 0;

    BaseType_t rval = xTaskNotifyFromISR(ncpRXTaskHandle, (ncpRXCurrBuff | NCP_NOTIFY),
                                         eSetValueWithoutOverwrite, &higherPriorityTaskWoken);
    if (rval == pdFALSE) {
      // previous packet still pending, 😬
      // TODO - track dropped packets?
      configASSERT(0);
    } else {
      // switch ncp buffers
      ncpRXCurrBuff ^= 1;
    }
  } while (0);

  return higherPriorityTaskWoken;
}

static inline BaseType_t postTxEventHandle(SerialHandle_t *handle, NCPNegotiatingEvent_t event)
    __attribute__((always_inline));
static inline BaseType_t postTxEventHandle(SerialHandle_t *handle,
                                           NCPNegotiatingEvent_t event) {
  BaseType_t ret = pdFALSE;

  switch (event) {
  case BAUD_NEGOTIATING_REQUEST:
    //Keep the negotiating lock  to avoid sending messages
    xTimerStartFromISR(ncp_ctx.negotiate_timer, &ret);
    break;
  case BAUD_NEGOTIATING_REPLY:
    serialSetBaudRate(handle, ncp_ctx.requested_baud);
    xSemaphoreGiveFromISR(ncp_ctx.negotiating_lock, NULL);
    break;
  case NCP_NEGOTIATING_NONE:
    break;
  }
  ncp_ctx.event = NCP_NEGOTIATING_NONE;

  return ret;
}

static void ncpPreTxCb(SerialHandle_t *handle) { // called from task context
  configASSERT(handle);
  if (ncp_rx) {
    xSemaphoreTake(ncp_serial_lock, portMAX_DELAY);
  }
  LL_EXTI_DisableIT_0_31(LL_EXTI_LINE_0);
  lpmPeripheralActive(LPM_USART3);
  LL_GPIO_SetPinMode(BM_INT_GPIO_Port, BM_INT_Pin, LL_GPIO_MODE_OUTPUT);
  IOWrite(&BM_INT, 0);
  vTaskDelay(pdMS_TO_TICKS(LPM_WAKE_TIME_MS));
}

static void ncpPostTxCb(SerialHandle_t *handle) { // called form ISR context
  BaseType_t higherPriorityTaskEventHandle = pdFALSE;
  BaseType_t higherPriorityTaskWoken = pdFALSE;
  configASSERT(handle);

  if (handle->arg) {
    higherPriorityTaskEventHandle =
        postTxEventHandle(handle, *(NCPNegotiatingEvent_t *)handle->arg);
  } else {
    xSemaphoreGiveFromISR(ncp_ctx.negotiating_lock, NULL);
  }
  LL_GPIO_SetPinMode(BM_INT_GPIO_Port, BM_INT_Pin, LL_GPIO_MODE_INPUT);
  LL_EXTI_EnableIT_0_31(LL_EXTI_LINE_0);
  LL_GPIO_SetPinPull(BM_INT_GPIO_Port, BM_INT_Pin, LL_GPIO_PULL_UP);
  // LPM_USART3_RX will get set on the next rising edge
  lpmPeripheralInactiveFromISR(LPM_USART3_TX);
  portYIELD_FROM_ISR(
      higherPriorityTaskWoken == pdTRUE || higherPriorityTaskEventHandle == pdTRUE ? pdTRUE
                                                                                   : pdFALSE);
}
