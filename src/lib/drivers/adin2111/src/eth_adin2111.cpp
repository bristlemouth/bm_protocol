#include <errno.h>
#include <string.h>

// Includes for FreeRTOS
#include "FreeRTOS.h"
#include "aligned_malloc.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"

#include "lwip/api.h"
#include "lwip/debug.h"
#include "lwip/ip.h"
#include "lwip/nd6.h"
#include "lwip/pbuf.h"
#include "lwip/prot/ip6.h"
#include "lwip/stats.h"
#include "lwip/udp.h"
#include "netif/ethernet.h"

#include "bcmp.h"
#include "bsp.h"
#include "task_priorities.h"

#include "gpio.h"
#include "pcap.h"

/* Extra 4 bytes for FCS and 2 bytes for the frame header */
#define MAX_FRAME_BUF_SIZE (MAX_FRAME_SIZE + 4 + 2)

#define PAUSE_RESUME_TASKS_TIMEOUT_MS (1000)

#define DMA_ALIGN_SIZE (4)

static TaskHandle_t serviceTask = NULL;
static uint8_t dev_mem[ADIN2111_DEVICE_SIZE];

static adin2111_DriverConfig_t drvConfig = {
    .pDevMem = (void *)dev_mem,
    .devMemSize = sizeof(dev_mem),
    .fcsCheckEn = false,
    .tsTimerPin = ADIN2111_TS_TIMER_MUX_NA,
    .tsCaptPin = ADIN2111_TS_CAPT_MUX_NA,
};
typedef struct {
  // NOTE: bufDesc MUST be first, since it's what the callback returns and we need
  // to know what address to free :D
  adi_eth_BufDesc_t bufDesc;
  adin2111_Port_e port; // TODO: can we just use ->port in bufDesc?
  adin2111_DeviceHandle_t dev;
} txMsgEvt_t;

typedef struct {
  // NOTE: bufDesc MUST be first, since it's what the callback returns and we need
  // to know what address to free :D
  adi_eth_BufDesc_t bufDesc;
  adin2111_DeviceHandle_t dev;
} rxMsgEvt_t;

typedef struct {
  adin2111_DeviceHandle_t handle;
  adin2111_Port_e port;
} linkChangeEvt_t;

typedef struct {
  adin2111_DeviceHandle_t handle;
  adin2111_Port_e port;
  adin2111_port_stats_callback_t callback;
  void *args;
} portStatsReqEvt_t;

static BmL2RxCb _rx_callback;
static BmL2DeviceLinkChangeCb _link_change_callback;

#define ETH_EVT_QUEUE_LEN 32

typedef enum {
  // Buffer ready to send to ADIN
  EVT_ETH_TX,

  // New buffer received from ADIN
  EVT_ETH_RX,

  // IRQ event callback
  EVT_IRQ_CB,

  // Link change
  EVT_LINK_CHANGE,

  // Port stats request
  EVT_PORT_STATS,

  // Adin task pause
  EVT_PAUSE_ADIN_TASK,

  // Adin task resume
  EVT_RESUME_ADIN_TASK,

} ethEvtType_e;

typedef struct {
  ethEvtType_e type;
  void *data;
} ethEvt_t;

// Queue used to handle all tx/rx/irq events
static QueueHandle_t _eth_evt_queue;
static bool _adin_thread_paused = false;
static rxMsgEvt_t *adin_rx_buf_mem[RX_QUEUE_NUM_ENTRIES];
static void free_tx_msg_req(txMsgEvt_t *txMsg);
static rxMsgEvt_t *createRxMsgReq(adin2111_DeviceHandle_t hDevice, uint16_t buf_len);
static bool resume_pause_adin_task(bool start, TaskHandle_t task_to_notify,
                                   uint32_t timeout_ms);

/*!
  ADIN message received callback

  \param pCBParam unused
  \param Event unused
  \param *pArg pointer to rxMsgEvt_t
  \return none
*/
static void adin2111_rx_cb(void *pCBParam, uint32_t Event, void *pArg) {
  (void)Event;
  (void)pCBParam;

  // pArg points to rxMsgEvt_t
  ethEvt_t event = {.type = EVT_ETH_RX, .data = pArg};
  configASSERT(xQueueSend(_eth_evt_queue, &event, 10));
}

/*!
  ADIN message transmitted callback (called after message is done transmitting)

  \param pCBParam unused
  \param Event unused
  \param *pArg pointer to txMsgEvt_t
  \return none
*/
static void adin2111_tx_cb(void *pCBParam, uint32_t Event, void *pArg) {
  (void)Event;
  (void)pCBParam;

  txMsgEvt_t *txMsg = static_cast<txMsgEvt_t *>(pArg);
  if (txMsg) {
    free_tx_msg_req(txMsg);
  } else {
    // Callback with empty arg! Uh oh
    configASSERT(0);
  }
}

/*!
  ADIN2111 link change event callback

  \param pCBParam unused
  \param Event unused
  \param *pArg pointer to adi_mac_StatusRegisters_t
  \return none
*/
static void adin2111_link_change_cb(void *pCBParam, uint32_t Event, void *pArg) {
  (void)Event;

  // Letting callback handle checking the port state for each port
  adi_mac_StatusRegisters_t *statusRegisters = static_cast<adi_mac_StatusRegisters_t *>(pArg);

  linkChangeEvt_t *linkChangeEvt =
      static_cast<linkChangeEvt_t *>(pvPortMalloc(sizeof(linkChangeEvt_t)));
  configASSERT(linkChangeEvt);

  linkChangeEvt->handle = (adin2111_DeviceHandle_t)pCBParam;

  if (statusRegisters->p1StatusMasked == ADI_PHY_EVT_LINK_STAT_CHANGE) {
    linkChangeEvt->port = ADIN2111_PORT_1;
  } else if (statusRegisters->p2StatusMasked == ADI_PHY_EVT_LINK_STAT_CHANGE) {
    linkChangeEvt->port = ADIN2111_PORT_2;
  } else {
    printf("Unknown link change event\n");
    vPortFree(linkChangeEvt);
    linkChangeEvt = NULL;
  }

  if (linkChangeEvt) {
    ethEvt_t event = {.type = EVT_LINK_CHANGE, .data = linkChangeEvt};
    configASSERT(xQueueSend(_eth_evt_queue, &event, 10));
  }
}

/*!
  Pauses / resumes the adin processing task

  \param start - true if task should be started, false if task should be paused
  \param task_to_notify - task to notify when task is paused/resumed
  \param timeout_ms - timeout in ms to wait for task to pause/resume
  \return none
*/
static bool resume_pause_adin_task(bool start, TaskHandle_t task_to_notify,
                                   uint32_t timeout_ms) {
  ethEvtType_e type = start ? EVT_RESUME_ADIN_TASK : EVT_PAUSE_ADIN_TASK;
  TaskHandle_t *task_to_notify_mem =
      static_cast<TaskHandle_t *>(pvPortMalloc(sizeof(TaskHandle_t)));
  configASSERT(task_to_notify_mem);
  *task_to_notify_mem = task_to_notify;
  ethEvt_t event = {.type = type, .data = task_to_notify_mem};
  if (xQueueSend(_eth_evt_queue, &event, timeout_ms) != pdPASS) {
    vPortFree(task_to_notify_mem);
    return false;
  }
  if (!ulTaskNotifyTake(pdTRUE, timeout_ms)) {
    configASSERT(false); // We should never get here.
  }
  return true;
}

/*!
  Link change callback function. Whenever ADIN2111 ports change, this function
  is called in the main thread. Use to notify interested tasks about change

  \param linkChangeEvt struct with link change information
  \return none
*/
void _link_change(linkChangeEvt_t *linkChangeEvt) {
  adi_eth_LinkStatus_e status;
  if (adin2111_GetLinkStatus(linkChangeEvt->handle, linkChangeEvt->port, &status) ==
      ADI_ETH_SUCCESS) {

    // Pass data to callback function if present
    if (_link_change_callback) {
      _link_change_callback((void *)linkChangeEvt->handle, linkChangeEvt->port,
                            (status == ADI_ETH_LINK_STATUS_UP));
    }
  }
}

/*!
  Read adin port statistics for a specific port

  \param portStatsReqEvt_t struct with device/port information
  \return none
*/
void _get_port_stats(portStatsReqEvt_t *req) {
  configASSERT(req->handle);
  configASSERT(req->callback);

  adin_port_stats_t *stats =
      static_cast<adin_port_stats_t *>(pvPortMalloc(sizeof(adin_port_stats_t)));
  memset(stats, 0, sizeof(adin_port_stats_t));
  do {

    if (adin2111_GetMseLinkQuality(req->handle, req->port, &stats->mse_link_quality) !=
        ADI_ETH_SUCCESS) {
      printf("Error reading adin stats\n");
      break;
    }

    if (adin2111_FrameChkReadRxErrCnt(req->handle, req->port, &stats->frame_check_rx_err_cnt) !=
        ADI_ETH_SUCCESS) {
      printf("Error reading adin stats\n");
      break;
    }

    if (adin2111_FrameChkReadErrorCnt(req->handle, req->port,
                                      &stats->frame_check_err_counters) != ADI_ETH_SUCCESS) {
      printf("Error reading adin stats\n");
      break;
    }

    // Let the requester know data is ready!
    req->callback((adin2111_DeviceHandle_t)req->handle, req->port, stats, req->args);
  } while (0);

  vPortFree(stats);
}

/*!
  ADIN2111 main event thread

  Rx, Tx and pin IRQ events are all handled by this thread.

  \param parameters unused
  \return none
*/
static void adin2111_thread(void *parameters) {
  (void)parameters;

  while (1) {
    ethEvt_t event;
    if (!xQueueReceive(_eth_evt_queue, &event, portMAX_DELAY)) {
      printf("Error receiving from queue\n");
      continue;
    }

    // Adin thread is paused.
    if (_adin_thread_paused && event.type != EVT_RESUME_ADIN_TASK) {
      // If thread is paused, only EVT_RESUME_ADIN_TASK events are allowed
      printf("Adin paused, ignoring event %d\n", event.type);
      switch (event.type) {
      case EVT_ETH_TX: {
        free_tx_msg_req(static_cast<txMsgEvt_t *>(event.data));
        break;
      }
      case EVT_ETH_RX: {
        // Sometimes we get a RX event when we're paused. In order to make
        // sure we don't leak memory, lets resubmit the buffer just like
        // we would do if the adin was not paused.
        rxMsgEvt_t *rxMsg = static_cast<rxMsgEvt_t *>(event.data);
        // Re-submit buffer into ADIN's RX queue
        adi_eth_Result_e result = adin2111_SubmitRxBuffer(rxMsg->dev, &rxMsg->bufDesc);
        if (result != ADI_ETH_SUCCESS) {
          printf("Unable to re-submit RX Buffer\n");
          configASSERT(0);
        }
        break;
      }
      default: {
        if (event.data) {
          vPortFree(event.data);
        }
        break;
      }
      }
      continue;
    }

    // Adin thread is running.
    switch (event.type) {

    case EVT_ETH_TX: {
      txMsgEvt_t *txMsg = static_cast<txMsgEvt_t *>(event.data);

      if (!txMsg) {
        printf("Empty tx data :(\n");
        break;
      }

      adi_eth_Result_e result =
          adin2111_SubmitTxBuffer(txMsg->dev, (adin2111_TxPort_e)txMsg->port, &txMsg->bufDesc);
      if (result != ADI_ETH_SUCCESS) {
        // Free all the buffers!
        free_tx_msg_req(txMsg);

        /* Failed to submit TX Buffer? */
        printf("Unable to submit TX buffer\n");
        break;
      }

      // NOTE: txMsg will be freed in adin2111_tx_cb

      break;
    }

    case EVT_ETH_RX: {
      rxMsgEvt_t *rxMsg = static_cast<rxMsgEvt_t *>(event.data);

      pcapTxPacket(rxMsg->bufDesc.pBuf, rxMsg->bufDesc.trxSize);

      uint8_t rx_port_mask = (1 << rxMsg->bufDesc.port);
      BmErr retv = (BmErr)_rx_callback(rxMsg->dev, rxMsg->bufDesc.pBuf, rxMsg->bufDesc.trxSize,
                                       rx_port_mask);
      if (retv != BmOK) {
        printf("Unable to pass to the L2 layer\n");
        // Don't break since we still want to re-add it to the adin rx queue below
      }

      // Re-submit buffer into ADIN's RX queue
      adi_eth_Result_e result = adin2111_SubmitRxBuffer(rxMsg->dev, &rxMsg->bufDesc);
      if (result != ADI_ETH_SUCCESS) {
        printf("Unable to re-submit RX Buffer\n");
        configASSERT(0);
      }

      break;
    }

    case EVT_IRQ_CB: {
      adi_bsp_irq_callback();
      break;
    }

    case EVT_LINK_CHANGE: {
      configASSERT(event.data);
      linkChangeEvt_t *linkChangeEvt = static_cast<linkChangeEvt_t *>(event.data);
      _link_change(linkChangeEvt);
      vPortFree(linkChangeEvt);
      break;
    }

    case EVT_PORT_STATS: {
      configASSERT(event.data);
      portStatsReqEvt_t *portStatsReqEvt = static_cast<portStatsReqEvt_t *>(event.data);
      _get_port_stats(portStatsReqEvt);
      vPortFree(portStatsReqEvt);
      break;
    }

    case EVT_PAUSE_ADIN_TASK: {
      configASSERT(event.data);
      _adin_thread_paused = true;
      TaskHandle_t *task_to_notify = static_cast<TaskHandle_t *>(event.data);
      xTaskNotifyGive(*task_to_notify);
      vPortFree(task_to_notify);
      break;
    }

    case EVT_RESUME_ADIN_TASK: {
      configASSERT(event.data);
      _adin_thread_paused = false;
      TaskHandle_t *task_to_notify = static_cast<TaskHandle_t *>(event.data);
      xTaskNotifyGive(*task_to_notify);
      vPortFree(task_to_notify);
      break;
    }

    default: {
      // Unexpected event!
      configASSERT(0);
    }
    }
  }
}

/*!
  ADIN_INT interrupt callback. Schedules EVT_IRQ_CB event for handling in main queue

  \param *pxHigherPriorityTaskWoken Let ISR task know that we need to wake up the main thread
  \return none
*/
static void _irq_evt_cb_from_isr(BaseType_t *pxHigherPriorityTaskWoken) {
  ethEvt_t event = {.type = EVT_IRQ_CB, .data = NULL};

  // Since it's an interrupt, send it to the front of the queue to be handled promptly
  configASSERT(xQueueSendToFrontFromISR(_eth_evt_queue, &event, pxHigherPriorityTaskWoken));
}

/*!
  ADIN2111 Hardware initialization

  \param hDevice adin device handle
  \param rx_callback Callback function to be called when new data is received
  \param link_change_callback Callback function to be called when link state changes
  \return 0 if successful, nonzero otherwise
*/
BmErr adin2111_hw_init(void *device, BmL2RxCb rx_callback,
                       BmL2DeviceLinkChangeCb link_change_callback, uint8_t enabled_port_mask) {
  BmErr result = BmOK;
  BaseType_t rval;
  adin2111_DeviceHandle_t hDevice = (adin2111_DeviceHandle_t)device;

  configASSERT(rx_callback);
  _rx_callback = rx_callback;

  // Optional link change callback
  _link_change_callback = link_change_callback;

  do {
    /* Initialize BSP to kickoff thread to service GPIO and SPI DMA interrupts
        TODO: Provide a SPI interface? */
    if (adi_bsp_init()) {
      result = BmENOENT;
      break;
    }

    _eth_evt_queue = xQueueCreate(ETH_EVT_QUEUE_LEN, sizeof(ethEvt_t));
    configASSERT(_eth_evt_queue);

    adi_bsp_register_irq_evt(_irq_evt_cb_from_isr);

    /* ADIN2111 Init process */
    if (adin2111_Init(hDevice, &drvConfig) != ADI_ETH_SUCCESS) {
      result = BmENODEV;
      break;
    }

    /* Register Callback for link change */
    if (adin2111_RegisterCallback(hDevice, adin2111_link_change_cb, ADI_MAC_EVT_LINK_CHANGE) !=
        ADI_ETH_SUCCESS) {
      result = BmENOMEM;
      break;
    }

    // Allocate RX buffers for ADIN (Only need to do this once)
    for (uint32_t idx = 0; idx < RX_QUEUE_NUM_ENTRIES; idx++) {
      adin_rx_buf_mem[idx] = createRxMsgReq(hDevice, MAX_FRAME_BUF_SIZE);
      configASSERT(adin_rx_buf_mem[idx]);

      // Submit rx buffer to ADIN's RX queue
      // Once buffer is used, adin2111_rx_cb will be called
      // and it can be re-added to the queue with this same function
      if (adin2111_SubmitRxBuffer(hDevice, &adin_rx_buf_mem[idx]->bufDesc) != ADI_ETH_SUCCESS) {
        result = BmENOMEM;
        printf("Unable to re-submit RX Buffer\n");
        configASSERT(0);
        break;
      }
    }

    // Failed to initialize
    if (result != BmOK) {
      break;
    }

    /* Confirm device configuration */
    if (adin2111_SyncConfig(hDevice) != ADI_ETH_SUCCESS) {
      result = BmETIMEDOUT;
      break;
    }
    rval = xTaskCreate(adin2111_thread, "ADIN2111 Service Thread", 8192, NULL,
                       ADIN_SERVICE_TASK_PRIORITY, &serviceTask);
    configASSERT(rval == pdTRUE);

    adin2111_hw_start(hDevice, enabled_port_mask);
  } while (0);

  return result;
}

typedef struct {
  struct eth_hdr eth_hdr;
  struct ip6_hdr ip6_hdr;
  uint8_t payload[0];
} net_header_t;

/*!
  Add egress port to IP address and update UDP checksum

  \param buff buffer with frame
  \param port port in which frame is going out of
  \return none
*/
static void add_egress_port(uint8_t *buff, uint8_t port) {
  configASSERT(buff);

  net_header_t *header = reinterpret_cast<net_header_t *>(buff);

  // Modify egress port byte in IP address
  uint8_t *pbyte = (uint8_t *)&header->ip6_hdr.src;
  pbyte[egress_port_idx] = port;

  //
  // Correct checksum to account for change in ip address
  //
  if (header->eth_hdr.type == ETHTYPE_IPV6) {
    if (header->ip6_hdr._nexth == IP_PROTO_UDP) {
      struct udp_hdr *udp_hdr = reinterpret_cast<struct udp_hdr *>(header->payload);
      // Undo 1's complement
      udp_hdr->chksum ^= 0xFFFF;

      // Add port to checksum (we can only do this because the value was previously 0)
      // Since udp checksum is sum of uint16_t bytes
      udp_hdr->chksum += port;

      // Do 1's complement again
      udp_hdr->chksum ^= 0xFFFF;
    } else if (header->ip6_hdr._nexth == ip_proto_bcmp) {
      // fix checksum for BCMP
    }
  }
}

/*!
  Allocate rx message request and buffer for adin

  NOTE: these RX messages will never be freed, but instead re-queued in the adin driver

  \param hDevice adin device handle
  \param buf_len buffer size to be allocated

  \return pointer to allocated message request
*/
static rxMsgEvt_t *createRxMsgReq(adin2111_DeviceHandle_t hDevice, uint16_t buf_len) {
  rxMsgEvt_t *rxMsg = static_cast<rxMsgEvt_t *>(pvPortMalloc(sizeof(rxMsgEvt_t)));
  if (rxMsg) {
    memset(rxMsg, 0x00, sizeof(rxMsgEvt_t));
    rxMsg->dev = hDevice;
    rxMsg->bufDesc.bufSize = buf_len;
    rxMsg->bufDesc.cbFunc = adin2111_rx_cb;

    // Allocate alligned buffer in case we use DMA
    rxMsg->bufDesc.pBuf = static_cast<uint8_t *>(aligned_malloc(DMA_ALIGN_SIZE, buf_len));
    if (rxMsg->bufDesc.pBuf) {
      // Clear buffer
      // TODO - use pbuf instead of malloc/copying
      memset(rxMsg->bufDesc.pBuf, 0x00, buf_len);
    } else {
      vPortFree(rxMsg);
      rxMsg = NULL;
    }
  }

  return rxMsg;
}

/*!
  Allocate and initialize tx message request and copy data to data buffer

  NOTE: txMsgReq MUST be freed with free_tx_msg_req since it has an aligned buffer internally

  \param hDevice adin device handle
  \param buf data buffer
  \param buf_len buffer length
  \param port ADIN port to transmit message on
  \return pointer to txMsgEvt
*/
static txMsgEvt_t *createTxMsgReq(adin2111_DeviceHandle_t hDevice, uint8_t *buf,
                                  uint16_t buf_len, adin2111_Port_e port) {
  configASSERT(buf);

  txMsgEvt_t *txMsg = static_cast<txMsgEvt_t *>(pvPortMalloc(sizeof(txMsgEvt_t)));
  if (txMsg) {
    memset(txMsg, 0x00, sizeof(txMsgEvt_t));
    txMsg->dev = hDevice;
    txMsg->port = port;
    txMsg->bufDesc.trxSize = buf_len;
    txMsg->bufDesc.cbFunc = adin2111_tx_cb;

    // Allocate alligned buffer in case we use DMA
    txMsg->bufDesc.pBuf = static_cast<uint8_t *>(aligned_malloc(DMA_ALIGN_SIZE, buf_len));
    if (txMsg->bufDesc.pBuf) {
      // Copy data to buffer
      // TODO - use pbuf instead of malloc/copying
      memcpy(txMsg->bufDesc.pBuf, buf, buf_len);
    } else {
      vPortFree(txMsg);
      txMsg = NULL;
    }
  }

  return txMsg;
}

/*!
  Free tx message request and data buffer

  \param txMsg pointer to txMsgEvet_t to be freed
  \return none
*/
static void free_tx_msg_req(txMsgEvt_t *txMsg) {
  if (txMsg) {
    if (txMsg->bufDesc.pBuf) {
      aligned_free(txMsg->bufDesc.pBuf);
    }
    vPortFree(txMsg);
  }
}

/*!
  ADIN TX function

  \param hDevice adin device handle
  \param buf data buffer
  \param buf_len buffer length
  \param port_mask which ports will this be sent over
  \param port_offset 🤷‍♂️ (TODO - figure out why this is)
  \return none
*/
BmErr adin2111_tx(void *device, uint8_t *buf, uint16_t buf_len, uint8_t port_mask,
                  uint8_t port_offset) {
  BmErr retv = BmOK;
  adin2111_DeviceHandle_t hDevice = (adin2111_DeviceHandle_t)device;

  do {
    if (!hDevice) {
      // no device provided!
      retv = BmEINVAL;
      break;
    }

    if (!buf) {
      retv = BmEINVAL;
      break;
    }

    for (uint32_t port = 0; port < ADIN2111_PORT_NUM; port++) {
      if (port_mask & (0x01 << port)) {
        txMsgEvt_t *txMsg =
            createTxMsgReq(hDevice, buf, buf_len, static_cast<adin2111_Port_e>(port));
        if (txMsg) {
          /* We are modifying the IPV6 SRC address to include the egress port */
          uint8_t bm_egress_port = (0x01 << port) << port_offset;
          add_egress_port(txMsg->bufDesc.pBuf, bm_egress_port);

          pcapTxPacket(buf, buf_len);

          ethEvt_t event = {.type = EVT_ETH_TX, .data = txMsg};
          if (xQueueSend(_eth_evt_queue, &event, 100) == pdFALSE) {
            free_tx_msg_req(txMsg);
            retv = BmENOMEM;
            break;
          }

        } else {
          retv = BmENOMEM;
          break;
        }
      }
    }

    if (retv != BmOK) {
      break;
    }
  } while (0);

  return retv;
}

/*!
 Enables the ADIN2111 interface
 \param dev adin device handle
 \return 0 on success
 */
int adin2111_hw_start(adin2111_DeviceHandle_t dev, uint8_t port_mask) {
  adi_eth_Result_e res = ADI_ETH_SUCCESS;
  for (uint8_t port = 0; port < ADIN2111_PORT_NUM; port++) {
    if (port_mask & (0x01 << port)) {
      res = adin2111_EnablePort(dev, static_cast<adin2111_Port_e>(port));
      if (res != ADI_ETH_SUCCESS) {
        break;
      }
    }
  }
  return res;
}

/*!
  Disables the ADIN2111 interface
  \param dev adin device handle
  \return 0 on success
 */
int adin2111_hw_stop(adin2111_DeviceHandle_t dev, uint8_t port_mask) {
  adi_eth_Result_e res = ADI_ETH_SUCCESS;
  for (uint8_t port = 0; port < ADIN2111_PORT_NUM; port++) {
    if (port_mask & (0x01 << port)) {
      res = adin2111_DisablePort(dev, static_cast<adin2111_Port_e>(port));
      if (res != ADI_ETH_SUCCESS) {
        break;
      }
    }
  }
  return res;
}

/*!
  Get ADIN Port stats.
  NOTE: This function does not return the stats, but a callback function is called
  with them when they are ready

  \param dev - adin device handle
  \param port - adin port to get stats from
  \param callback - callback function to call with the results
  \param *args -  args to pass to callback function
  \return true if request successful, false otherwise
*/
bool adin2111_get_port_stats(adin2111_DeviceHandle_t dev, adin2111_Port_e port,
                             adin2111_port_stats_callback_t callback, void *args) {
  configASSERT(dev);
  configASSERT(callback);
  configASSERT(port < ADIN2111_PORT_NUM);

  portStatsReqEvt_t *portStatsReqEvt =
      static_cast<portStatsReqEvt_t *>(pvPortMalloc(sizeof(portStatsReqEvt_t)));
  configASSERT(portStatsReqEvt);
  portStatsReqEvt->handle = dev;
  portStatsReqEvt->port = port;
  portStatsReqEvt->callback = callback;
  portStatsReqEvt->args = args;

  ethEvt_t event = {.type = EVT_PORT_STATS, .data = portStatsReqEvt};
  bool rval = false;

  if (xQueueSend(_eth_evt_queue, &event, 10) == pdTRUE) {
    rval = true;
  } else {
    // Free port stats request event since we were unable to queue the request
    vPortFree(portStatsReqEvt);
  }

  return rval;
}

BmErr adin2111_power_cb(const void *devHandle, bool on, uint8_t port_mask) {
  BmErr rval = BmOK;
  int err = 0;
  adin2111_DeviceHandle_t hDevice =
      reinterpret_cast<adin2111_DeviceHandle_t>(const_cast<void *>(devHandle));
  if (on) {
    do {
      IOWrite(&ADIN_PWR, 1);
      err = adin2111_Init(hDevice, &drvConfig);
      if (err != ADI_ETH_SUCCESS) {
        rval = BmENODEV;
        break;
      }

      err =
          adin2111_RegisterCallback(hDevice, adin2111_link_change_cb, ADI_MAC_EVT_LINK_CHANGE);
      if (err != ADI_ETH_SUCCESS) {
        rval = BmENOMEM;
        break;
      }
      for (uint32_t idx = 0; idx < RX_QUEUE_NUM_ENTRIES; idx++) {
        // Submit rx buffer to ADIN's RX queue
        err = adin2111_SubmitRxBuffer(hDevice, &adin_rx_buf_mem[idx]->bufDesc);
        if (err != ADI_ETH_SUCCESS) {
          printf("Unable to re-submit RX Buffer\n");
          rval = BmENOMEM;
          configASSERT(0);
          break;
        }
      }
      // Failed to initialize
      if (err != ADI_ETH_SUCCESS) {
        rval = BmENODEV;
        break;
      }
      err = adin2111_SyncConfig(hDevice);
      if (err != ADI_ETH_SUCCESS) {
        rval = BmETIMEDOUT;
        break;
      }
      err = adin2111_hw_start(hDevice, port_mask);
      if (err != ADI_ETH_SUCCESS) {
        rval = BmENODEV;
        break;
      }
      if (!resume_pause_adin_task(true, xTaskGetCurrentTaskHandle(),
                                  PAUSE_RESUME_TASKS_TIMEOUT_MS)) {
        rval = BmECANCELED;
        break;
      }
    } while (0);
    if (err != ADI_ETH_SUCCESS) {
      IOWrite(&ADIN_PWR, 0);
    }
  } else {
    do {
      if (!resume_pause_adin_task(false, xTaskGetCurrentTaskHandle(),
                                  PAUSE_RESUME_TASKS_TIMEOUT_MS)) {
        rval = BmECANCELED;
        break;
      }
      err = adin2111_hw_stop(hDevice, port_mask);
      if (err != ADI_ETH_SUCCESS) {
        rval = BmEALREADY;
        break;
      }
      IOWrite(&ADIN_PWR, 0);
    } while (0);
  }
  return rval;
}
