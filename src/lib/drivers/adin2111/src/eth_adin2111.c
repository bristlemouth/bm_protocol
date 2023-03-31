#include <errno.h>
#include <string.h>

// Includes for FreeRTOS
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"
#include "aligned_malloc.h"

#include "netif/ethernet.h"
#include "lwip/pbuf.h"
#include "lwip/api.h"
#include "lwip/debug.h"
#include "lwip/stats.h"
#include "lwip/nd6.h"
#include "lwip/prot/ip6.h"
#include "lwip/udp.h"

#include "adi_bsp.h"
#include "eth_adin2111.h"
#include "bm_l2.h"
#include "task_priorities.h"
#include "bsp.h"

#include "pcap.h"

#define DMA_ALIGN_SIZE (4)

static TaskHandle_t serviceTask = NULL;
static uint8_t dev_mem[ADIN2111_DEVICE_SIZE];

static adin2111_DriverConfig_t drvConfig = {
    .pDevMem = (void *)dev_mem,
    .devMemSize = sizeof(dev_mem),
    .fcsCheckEn = false,
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

static adin_rx_callback_t _rx_callback = NULL;

#define ETH_EVT_QUEUE_LEN 32

typedef enum {
    // Buffer ready to send to ADIN
    EVT_ETH_TX,

    // New buffer received from ADIN
    EVT_ETH_RX,

    // IRQ event callback
    EVT_IRQ_CB,

} ethEvtType_e;

typedef struct {
    ethEvtType_e type;
    void *data;
} ethEvt_t;

// Queue used to handle all tx/rx/irq events
static QueueHandle_t    _eth_evt_queue;

static void free_tx_msg_req(txMsgEvt_t *txMsg);
static rxMsgEvt_t *createRxMsgReq(adin2111_DeviceHandle_t hDevice, uint16_t buf_len);


/*!
  ADIN message received callback

  \param pCBParam unused
  \param Event unused
  \param *pArg pointer to rxMsgEvt_t
  \return none
*/
static void adin2111_rx_cb(void *pCBParam, uint32_t Event, void *pArg) {
    (void) Event;
    (void)pCBParam;

    // pArg points to rxMsgEvt_t
    ethEvt_t event = {.type=EVT_ETH_RX, .data=pArg};
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
    (void) Event;
    (void) pCBParam;

    txMsgEvt_t *txMsg = (txMsgEvt_t *)pArg;
    if(txMsg) {
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
    (void) Event;
    (void) pCBParam;

    adi_mac_StatusRegisters_t *statusRegisters = (adi_mac_StatusRegisters_t *)pArg;

    /* The port where the link status changed happened can be determined by */
    /* examining the values of the masked status registers. Then the link   */
    /* status can be read from the actual PHY registers. */

    // TODO - do something about it
    printf("LINK CHANGE\n");
    (void) statusRegisters;
}

/*!
  ADIN2111 main event thread

  Rx, Tx and pin IRQ events are all handled by this thread.

  \param parameters unused
  \return none
*/
static void adin2111_thread(void *parameters) {
    (void) parameters;

    while (1) {
        ethEvt_t event;
        if(!xQueueReceive(_eth_evt_queue, &event, portMAX_DELAY)) {
            printf("Error receiving from queue\n");
            continue;
        }

        switch(event.type) {
            case EVT_ETH_TX: {
                txMsgEvt_t *txMsg = (txMsgEvt_t *)event.data;

                if(!txMsg){
                    printf("Empty tx data :(\n");
                    break;
                }

                adi_eth_Result_e result = adin2111_SubmitTxBuffer(txMsg->dev, (adin2111_TxPort_e)txMsg->port, &txMsg->bufDesc);
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
                rxMsgEvt_t *rxMsg = (rxMsgEvt_t *)event.data;

                pcapTxPacket(rxMsg->bufDesc.pBuf, rxMsg->bufDesc.trxSize);

                uint8_t rx_port_mask = (1 << rxMsg->bufDesc.port);
                err_t retv =  _rx_callback(rxMsg->dev, rxMsg->bufDesc.pBuf, rxMsg->bufDesc.trxSize, rx_port_mask);
                if (retv != ERR_OK) {
                    printf("Unable to pass to the L2 layer\n");
                    // Don't break since we still want to re-add it to the adin rx queue below
                }

                // Re-submit buffer into ADIN's RX queue
                adi_eth_Result_e result = adin2111_SubmitRxBuffer(rxMsg->dev, &rxMsg->bufDesc);
                if (result != ADI_ETH_SUCCESS) {
                    printf("Unable to re-submit RX Buffer\n");
                }

                break;
            }
            case EVT_IRQ_CB: {
                adi_bsp_irq_callback();
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
    ethEvt_t event = {.type=EVT_IRQ_CB, .data=NULL};

    // Since it's an interrupt, send it to the front of the queue to be handled promptly
    configASSERT(xQueueSendToFrontFromISR(_eth_evt_queue, &event, pxHigherPriorityTaskWoken));
}

/*!
  ADIN2111 Hardware initialization

  \param hDevice adin device handle
  \param rx_callback Callback function to be called when new data is received
  \return 0 if successful, nonzero otherwise
*/
adi_eth_Result_e adin2111_hw_init(adin2111_DeviceHandle_t hDevice, adin_rx_callback_t rx_callback) {
    adi_eth_Result_e result = ADI_ETH_SUCCESS;
    BaseType_t rval;

    configASSERT(rx_callback);
    _rx_callback = rx_callback;

    do {
        /* Initialize BSP to kickoff thread to service GPIO and SPI DMA interrupts
        TODO: Provide a SPI interface? */
        if (adi_bsp_init()) {
            result = ADI_ETH_HW_ERROR;
            break;
        }

        _eth_evt_queue = xQueueCreate(ETH_EVT_QUEUE_LEN, sizeof(ethEvt_t));
        configASSERT(_eth_evt_queue);

        adi_bsp_register_irq_evt (_irq_evt_cb_from_isr);

        /* ADIN2111 Init process */
        result = adin2111_Init(hDevice, &drvConfig);
        if (result != ADI_ETH_SUCCESS) {
            break;
        }

        /* Register Callback for link change */
        result = adin2111_RegisterCallback(hDevice, adin2111_link_change_cb,
                        ADI_MAC_EVT_LINK_CHANGE);
        if (result != ADI_ETH_SUCCESS) {
            break;
        }

        // Allocate RX buffers for ADIN (Only need to do this once)
        for(uint32_t idx = 0; idx < RX_QUEUE_NUM_ENTRIES; idx++) {
            rxMsgEvt_t *rxMsg = createRxMsgReq(hDevice, MAX_FRAME_BUF_SIZE);
            configASSERT(rxMsg);

            // Submit rx buffer to ADIN's RX queue
            // Once buffer is used, adin2111_rx_cb will be called
            // and it can be re-added to the queue with this same function
            result = adin2111_SubmitRxBuffer(hDevice, &rxMsg->bufDesc);
            if (result != ADI_ETH_SUCCESS) {
                printf("Unable to re-submit RX Buffer\n");
                configASSERT(0);
                break;
            }
        }

        // Failed to initialize
        if(result != ADI_ETH_SUCCESS) {
            break;
        }

        /* Confirm device configuration */
        result = adin2111_SyncConfig(hDevice);
        if (result != ADI_ETH_SUCCESS) {
            break;
        }
        rval = xTaskCreate(adin2111_thread,
                       "ADIN2111 Service Thread",
                       8192,
                       NULL,
                       ADIN_SERVICE_TASK_PRIORITY,
                       &serviceTask);
        configASSERT(rval == pdTRUE);

        adin2111_hw_start(&hDevice);
    } while (0);

    return result;
}

typedef struct {
    struct eth_hdr eth_hdr;
    struct ip6_hdr ip6_hdr;
    struct udp_hdr udp_hdr;
} net_header_t;

/*!
  Add egress port to IP address and update UDP checksum

  \param buff buffer with frame
  \param port port in which frame is going out of
  \return none
*/
static void add_egress_port(uint8_t *buff, uint8_t port) {
    configASSERT(buff);

    net_header_t *header = (net_header_t *)buff;

    // Modify egress port byte in IP address
    uint8_t *pbyte = (uint8_t *)&header->ip6_hdr.src;
    pbyte[EGRESS_PORT_IDX] = port;

    //
    // Correct checksum to account for change in ip address
    //

    // Undo 1's complement
    header->udp_hdr.chksum ^= 0xFFFF;

    // Add port to checksum (we can only do this because the value was previously 0)
    // Since udp checksum is sum of uint16_t bytes
    header->udp_hdr.chksum += port;

    // Do 1's complement again
    header->udp_hdr.chksum ^= 0xFFFF;
}

/*!
  Allocate rx message request and buffer for adin

  NOTE: these RX messages will never be freed, but instead re-queued in the adin driver

  \param hDevice adin device handle
  \param buf_len buffer size to be allocated

  \return pointer to allocated message request
*/
static rxMsgEvt_t *createRxMsgReq(adin2111_DeviceHandle_t hDevice, uint16_t buf_len) {
    rxMsgEvt_t *rxMsg = (rxMsgEvt_t *)pvPortMalloc(sizeof(rxMsgEvt_t));
    if(rxMsg) {
        memset(rxMsg, 0x00, sizeof(rxMsgEvt_t));
        rxMsg->dev = hDevice;
        rxMsg->bufDesc.bufSize = buf_len;
        rxMsg->bufDesc.cbFunc = adin2111_rx_cb;

        // Allocate alligned buffer in case we use DMA
        rxMsg->bufDesc.pBuf = (uint8_t *)aligned_malloc(DMA_ALIGN_SIZE, buf_len);
        if(rxMsg->bufDesc.pBuf) {
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
static txMsgEvt_t *createTxMsgReq(adin2111_DeviceHandle_t hDevice, uint8_t* buf, uint16_t buf_len, adin2111_Port_e port) {
    configASSERT(buf);

    txMsgEvt_t *txMsg = (txMsgEvt_t *)pvPortMalloc(sizeof(txMsgEvt_t));
    if(txMsg) {
        memset(txMsg, 0x00, sizeof(txMsgEvt_t));
        txMsg->dev = hDevice;
        txMsg->port = port;
        txMsg->bufDesc.trxSize = buf_len;
        txMsg->bufDesc.cbFunc = adin2111_tx_cb;

        // Allocate alligned buffer in case we use DMA
        txMsg->bufDesc.pBuf = (uint8_t *)aligned_malloc(DMA_ALIGN_SIZE, buf_len);
        if(txMsg->bufDesc.pBuf) {
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
    if(txMsg) {
        if(txMsg->bufDesc.pBuf){
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
err_t adin2111_tx(adin2111_DeviceHandle_t hDevice, uint8_t* buf, uint16_t buf_len, uint8_t port_mask, uint8_t port_offset) {
    err_t retv = ERR_OK;

    do {
        if (!hDevice) {
            // no device provided!
            retv = ERR_IF;
            break;
        }

        if(!buf) {
            retv = ERR_BUF;
            break;
        }

        for(uint32_t port=0; port < ADIN2111_PORT_NUM; port++) {
            if (port_mask & (0x01 << port)) {
                txMsgEvt_t *txMsg = createTxMsgReq(hDevice, buf, buf_len, port);
                if (txMsg) {
                    /* We are modifying the IPV6 SRC address to include the egress port */
                    uint8_t bm_egress_port = (0x01 << port) << port_offset;
                    add_egress_port(txMsg->bufDesc.pBuf, bm_egress_port);

                    pcapTxPacket(buf, buf_len);

                    ethEvt_t event = {.type=EVT_ETH_TX, .data=txMsg};
                    if(xQueueSend(_eth_evt_queue, &event, 100) == pdFALSE) {
                        free_tx_msg_req(txMsg);
                        retv = ERR_MEM;
                        break;
                    }

                } else {
                    retv = ERR_MEM;
                    break;
                }
            }
        }

        if(retv != ERR_OK) {
            break;
        }
    } while(0);

    return retv;
}

/*!
 Enables the ADIN2111 interface
 \param *dev adin device handle
 \return 0 on success
 */
int adin2111_hw_start(adin2111_DeviceHandle_t* dev) {
    return adin2111_Enable(*dev);
}

/*!
  Disables the ADIN2111 interface
  \param *dev adin device handle
  \return 0 on success
 */
int adin2111_hw_stop(adin2111_DeviceHandle_t* dev) {
    return adin2111_Disable(*dev);
}
