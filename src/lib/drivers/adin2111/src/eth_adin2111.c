#include <errno.h>
#include <string.h>

#include "adi_bsp.h"
#include "eth_adin2111.h"
#include "bm_l2.h"
#include "task_priorities.h"
#include "bsp.h"

// Includes for FreeRTOS
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"

#include "netif/ethernet.h"
#include "lwip/pbuf.h"
#include "lwip/api.h"
#include "lwip/debug.h"
#include "lwip/stats.h"
#include "lwip/nd6.h"
#include "lwip/prot/ip6.h"
#include "lwip/udp.h"

#include "pcap.h"

static TaskHandle_t serviceTask = NULL;
static uint8_t dev_mem[ADIN2111_DEVICE_SIZE];

static adin2111_DriverConfig_t drvConfig = {
    .pDevMem = (void *)dev_mem,
    .devMemSize = sizeof(dev_mem),
    .fcsCheckEn = false,
};

static adin_rx_callback_t _rx_callback = NULL;

queue_t txQueue;
queue_t rxQueue;

DMA_BUFFER_ALIGN(static uint8_t txQueueBuf[QUEUE_NUM_ENTRIES][MAX_FRAME_BUF_SIZE], 4);
DMA_BUFFER_ALIGN(static uint8_t rxQueueBuf[QUEUE_NUM_ENTRIES][MAX_FRAME_BUF_SIZE], 4);
static adi_eth_BufDesc_t txBufDesc[QUEUE_NUM_ENTRIES];
static adi_eth_BufDesc_t rxBufDesc[QUEUE_NUM_ENTRIES];

static void adin2111_main_queue_init(adin2111_DeviceHandle_t hDevice, queue_t *pQueue, adi_eth_BufDesc_t bufDesc[QUEUE_NUM_ENTRIES], uint8_t buf[QUEUE_NUM_ENTRIES][MAX_FRAME_BUF_SIZE]);
static uint32_t adin2111_main_queue_available(queue_t *pQueue);
static bool adin2111_main_queue_is_full(queue_t *pQueue);
static bool adin2111_main_queue_is_empty(queue_t *pQueue);
static queue_entry_t *adin2111_main_queue_tail(queue_t *pQueue);
static queue_entry_t *adin2111_main_queue_head(queue_t *pQueue);
static void adin2111_main_queue_add(queue_t *pQueue, adin2111_Port_e port, adi_eth_BufDesc_t *pBufDesc, adi_eth_Callback_t cbFunc);
static void adin2111_main_queue_remove(queue_t *pQueue);

static void adin2111_rx_cb(void *pCBParam, uint32_t Event, void *pArg);
static void adin2111_tx_cb(void *pCBParam, uint32_t Event, void *pArg);
static void adin2111_link_change_cb(void *pCBParam, uint32_t Event, void *pArg);

static void adin2111_service_thread(void *parameters);

static QueueSetHandle_t             queue_set;
static QueueHandle_t                rx_port_queue;
static SemaphoreHandle_t            tx_ready_sem;

#define RX_PORT_QUEUE_LENGTH        (4)
#define BINARY_SEMAPHORE_LENGTH	    (1)

/* ====================================================================================================
    Main Queue Functions borrowed from ADIN2111 Example
   ====================================================================================================
*/

/**
 * @brief Initialize ADIN2111 Queue
 *
 * @note Used to initialize both TX and RX queues
 *
 * @param *pQueue Queue to initialize
 * @param bufDesc Descriptions for each buffer in Queue
 * @param buf 2D Buffers (either RX or TX)
 * @return none
 */
static void adin2111_main_queue_init(adin2111_DeviceHandle_t hDevice, queue_t *pQueue, adi_eth_BufDesc_t bufDesc[QUEUE_NUM_ENTRIES],
                                      uint8_t buf[QUEUE_NUM_ENTRIES][MAX_FRAME_BUF_SIZE])
{
    pQueue->head = 0;
    pQueue->tail = 0;
    pQueue->full = false;
    for (uint32_t i = 0; i < QUEUE_NUM_ENTRIES; i++) {
        pQueue->entries[i].dev = hDevice;
        pQueue->entries[i].pBufDesc = &bufDesc[i];
        pQueue->entries[i].pBufDesc->pBuf = &buf[i][0];
        pQueue->entries[i].pBufDesc->bufSize = MAX_FRAME_BUF_SIZE;
        pQueue->entries[i].pBufDesc->egressCapt = ADI_MAC_EGRESS_CAPTURE_NONE;
    }
}

/**
 * @brief Check how much available space in queue
 *
 * @note Returns number of available spots in queue
 *
 * @param *pQueue Queue to check
 * @return uint32_t Number of available spots in queue
 */
static uint32_t adin2111_main_queue_available(queue_t *pQueue)
{
    if (pQueue->full) {
        return 0;
    }

    uint32_t n = (pQueue->head + QUEUE_NUM_ENTRIES - pQueue->tail) % QUEUE_NUM_ENTRIES;
    n = QUEUE_NUM_ENTRIES - n;

    return n;
}

/**
 * @brief Check if queue is full
 *
 * @param *pQueue Queue to check
 * @return bool returns true on full queue
 */
static bool adin2111_main_queue_is_full(queue_t *pQueue)
{
    return pQueue->full;
}

/**
 * @brief Check if queue is empty
 *
 * @param *pQueue Queue to check
 * @return bool returns true on empty queue
 */
static bool adin2111_main_queue_is_empty(queue_t *pQueue)
{
    bool isEmpty = !pQueue->full && (pQueue->head == pQueue->tail);

    return isEmpty;
}

/**
 * @brief Returns tail entry of the Queue
 *
 * @param *pQueue Queue to check
 * @return queue_entry_t returns tail element of Queue
 */
static queue_entry_t *adin2111_main_queue_tail(queue_t *pQueue)
{
    queue_entry_t *p = NULL;

    if (!adin2111_main_queue_is_empty(pQueue)) {
        p = &pQueue->entries[pQueue->tail];
    }

    return p;
}

/**
 * @brief Returns head entry of the Queue
 *
 * @param *pQueue Queue to check
 * @return queue_entry_t returns head element of Queue
 */
static queue_entry_t *adin2111_main_queue_head(queue_t *pQueue)
{
    queue_entry_t *p = NULL;

    if (!adin2111_main_queue_is_full(pQueue)) {
        p = &pQueue->entries[pQueue->head];
    }

    return p;
}

/**
 * @brief Adds new entry to head of queue
 *
 * @param *pQueue Queue to add to
 * @param port (For TX Queue) Port to send out frame on
 * @param *pBufDesc Buffer Description to overwrite default values
 * @param cbFunc callback function on completion of TX or RX
 * @return queue_entry_t returns head element of Queue
 */
static void adin2111_main_queue_add(queue_t *pQueue, adin2111_Port_e port,
                    adi_eth_BufDesc_t *pBufDesc, adi_eth_Callback_t cbFunc)
{
    queue_entry_t *pEntry;

    pEntry = adin2111_main_queue_head(pQueue);

    if (adin2111_main_queue_available(pQueue) == 1) {
        pQueue->full = true;
    }

    pEntry->port = port;
    pEntry->pBufDesc->bufSize = pBufDesc->bufSize;
    pEntry->pBufDesc->cbFunc = cbFunc;
    pEntry->pBufDesc->trxSize = pBufDesc->trxSize;
    memcpy(pEntry->pBufDesc->pBuf, pBufDesc->pBuf, pBufDesc->bufSize);

    pEntry->sent = false;
    pQueue->head = (pQueue->head + 1) % QUEUE_NUM_ENTRIES;
}

/**
 * @brief Removes tail entry from queue
 *
 * @param *pQueue Queue to remove from
 * @return none
 */
static void adin2111_main_queue_remove(queue_t *pQueue)
{
    pQueue->full = false;

    pQueue->tail = (pQueue->tail + 1) % QUEUE_NUM_ENTRIES;
}

/* ====================================================================================================
    Relevant Callbacks
   ====================================================================================================
 */

static void adin2111_rx_cb(void *pCBParam, uint32_t Event, void *pArg)
{
    (void) Event;

    adi_eth_BufDesc_t *pBufDesc;
    rx_info_t rx_info;

    pBufDesc = (adi_eth_BufDesc_t *)pArg;
    rx_info.port = pBufDesc->port;
    rx_info.dev = (adin2111_DeviceHandle_t) pCBParam;

    xQueueSend(rx_port_queue, ( void * ) &rx_info, 0);

    pcapTxPacket(pBufDesc->pBuf, pBufDesc->trxSize);
}

static void adin2111_tx_cb(void *pCBParam, uint32_t Event, void *pArg)
{
    (void) Event;
    (void) pCBParam;
    (void) pArg;

    // adi_eth_BufDesc_t *pBufDesc;
    // pBufDesc = (adi_eth_BufDesc_t *)pArg;
}

static void adin2111_link_change_cb(void *pCBParam, uint32_t Event, void *pArg)
{
    (void) Event;
    (void) pCBParam;

    adi_mac_StatusRegisters_t statusRegisters = *(adi_mac_StatusRegisters_t *)pArg;

    /* The port where the link status changed happened can be determined by */
    /* examining the values of the masked status registers. Then the link   */
    /* status can be read from the actual PHY registers. */

    //LOG_INF("Link Change Callback entered");

    (void)statusRegisters;
}

/* ====================================================================================================
    Zephyr Integration functions
   ====================================================================================================
 */

static void adin2111_service_thread(void *parameters) {
    (void) parameters;

    adi_eth_Result_e result;
    queue_entry_t *pEntry;

    QueueSetMemberHandle_t activated_member;
    rx_info_t rx_info;
    uint8_t rx_port_mask;
    err_t retv;

    /* Create the queue set large enough to hold an event for every space in
    every queue and semaphore that is to be added to the set. */
    queue_set = xQueueCreateSet( RX_PORT_QUEUE_LENGTH + BINARY_SEMAPHORE_LENGTH );

    /* Create the queues and semaphores that will be contained in the set. */
    rx_port_queue = xQueueCreate( RX_PORT_QUEUE_LENGTH, sizeof(rx_info_t));

    /* Create the semaphore that is being added to the set. */
    tx_ready_sem = xSemaphoreCreateBinary();

    /* Check everything was created. */
    configASSERT( queue_set );
    configASSERT( rx_port_queue );
    configASSERT( tx_ready_sem );

    /* Add the queues and semaphores to the set.  Reading from these queues and
    semaphore can only be performed after a call to xQueueSelectFromSet() has
    returned the queue or semaphore handle from this point on. */
    xQueueAddToSet( rx_port_queue, queue_set );
    xQueueAddToSet( tx_ready_sem, queue_set );

    while (1) {
        /* Wait until there is something to do. */
        activated_member = xQueueSelectFromSet( queue_set, portMAX_DELAY );

        /* Perform a different action for each event type. */
        if (activated_member == tx_ready_sem){
            xSemaphoreTake(activated_member, 0);

            while (!adin2111_main_queue_is_empty(&txQueue)) {
                pEntry = adin2111_main_queue_tail(&txQueue);
                if ((pEntry != NULL) && (!pEntry->sent)) {
                    pEntry->sent = true;
                    result = adin2111_SubmitTxBuffer(pEntry->dev, (adin2111_TxPort_e)pEntry->port, pEntry->pBufDesc);
                    if (result == ADI_ETH_SUCCESS) {
                        // printf("Submitted TX Buf to ADIN2111\n");
                        adin2111_main_queue_remove(&txQueue);
                    } else {
                        /* Failed to submit TX Buffer? */
                        printf("Unable to submit TX buffer\n");
                        break;
                    }
                }
            }
        } else if (activated_member == rx_port_queue) {
            xQueueReceive(activated_member, &rx_info, 0);

            if (!adin2111_main_queue_is_empty(&rxQueue)) {
                pEntry = adin2111_main_queue_tail(&rxQueue);
                configASSERT(pEntry);

                rx_port_mask = (1 << rx_info.port);
                retv =  _rx_callback(rx_info.dev, pEntry->pBufDesc->pBuf, pEntry->pBufDesc->trxSize, rx_port_mask);
                if (retv != ERR_OK) {
                    printf("Unable to pass to the L2 layer");
                }

                /* Put the buffer back into queue and re-submit to the ADIN2111 driver */
                adin2111_main_queue_remove(&rxQueue);
                adin2111_main_queue_add(&rxQueue, rx_info.port, pEntry->pBufDesc, adin2111_rx_cb);
                result = adin2111_SubmitRxBuffer(pEntry->dev, pEntry->pBufDesc);
                if (result != ADI_ETH_SUCCESS) {
                    printf("Unable to re-submit RX Buffer\n");
                }
            }
        }
    }
}

adi_eth_Result_e adin2111_hw_init(adin2111_DeviceHandle_t hDevice, adin_rx_callback_t rx_callback) {
    adi_eth_Result_e result = ADI_ETH_SUCCESS;
    //adi_mac_AddressRule_t addrRule;
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

        /* ADIN2111 Init process */
        result = adin2111_Init(hDevice, &drvConfig);
        if (result != ADI_ETH_SUCCESS) {
            break;
        }

        // addrRule.VALUE16 = 0x0000;
        // addrRule.APPLY2PORT1 = 1;
        // addrRule.APPLY2PORT2 = 1;
        // addrRule.TO_HOST = 1;

        // result = adin2111_AddAddressFilter(hDevice, netif->hwaddr, NULL, addrRule);
        // if (result != ADI_ETH_SUCCESS) {
        //     configASSERT(0);
        // }

        /* Register Callback for link change */
        result = adin2111_RegisterCallback(hDevice, adin2111_link_change_cb,
                        ADI_MAC_EVT_LINK_CHANGE);
        if (result != ADI_ETH_SUCCESS) {
            break;
        }

        /* Prepare Rx buffers */
        adin2111_main_queue_init(hDevice, &txQueue, txBufDesc, txQueueBuf);
        adin2111_main_queue_init(hDevice, &rxQueue, rxBufDesc, rxQueueBuf);
        for (uint32_t i = 0; i < (QUEUE_NUM_ENTRIES/2); i++) {

            /* Set appropriate RX Callback */
            rxQueue.entries[(2*i)].pBufDesc->cbFunc = adin2111_rx_cb;
            rxQueue.entries[(2*i)+1].pBufDesc->cbFunc = adin2111_rx_cb;

            /* Submit buffers from both PORT1 and PORT2 to RX Queue */
            adin2111_main_queue_add(&rxQueue, ADIN2111_PORT_1, rxQueue.entries[2*i].pBufDesc, adin2111_rx_cb);
            adin2111_main_queue_add(&rxQueue, ADIN2111_PORT_2, rxQueue.entries[(2*i)+1].pBufDesc, adin2111_rx_cb);

            /* Submit the RX buffer ahead of time */
            adin2111_SubmitRxBuffer(hDevice, rxQueue.entries[2*i].pBufDesc);
            adin2111_SubmitRxBuffer(hDevice, rxQueue.entries[(2*i)+1].pBufDesc);
        }

        /* Confirm device configuration */
        result = adin2111_SyncConfig(hDevice);
        if (result != ADI_ETH_SUCCESS) {
            break;
        }
        rval = xTaskCreate(adin2111_service_thread,
                       "ADIN2111 Service Thread",
                       8192,
                       NULL,
                       ADIN_SERVICE_TASK_PRIORITY,
                       &serviceTask);
        configASSERT(rval == pdTRUE);

        /* This used to be started by zephyr after networking layer got set up, should we enable the ADIN here? */
        adin2111_hw_start(&hDevice);
    } while (0);

    return result;
}

typedef struct {
    struct eth_hdr eth_hdr;
    struct ip6_hdr ip6_hdr;
    struct udp_hdr udp_hdr;
} net_header_t;

void add_egress_port(uint8_t *buff, uint8_t port) {
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

err_t adin2111_tx(adin2111_DeviceHandle_t hDevice, uint8_t* buf, uint16_t buf_len, uint8_t port_mask, uint8_t port_offset) {
    queue_entry_t *pEntry;
    err_t retv = ERR_OK;

    if (!hDevice) {
        while(1) {
            printf("ADIN not found\n");
            for (int idx=0; idx < 10; idx++) {
#ifdef BSP_NUCLEO_U575
                IOWrite(&LED_RED, 1);
                vTaskDelay(100);
                IOWrite(&LED_RED, 0);
                IOWrite(&LED_GREEN, 1);
                vTaskDelay(100);
                IOWrite(&LED_GREEN, 0);
#endif // BSP_NUCLEO_U575
            }
        }
    }

    for(uint32_t port=0; port < ADIN2111_PORT_NUM; port++) {
        if (!adin2111_main_queue_is_full(&txQueue)) {
            pEntry = adin2111_main_queue_head(&txQueue);
            pEntry->pBufDesc->trxSize = buf_len;
            memcpy(pEntry->pBufDesc->pBuf, buf, pEntry->pBufDesc->trxSize);
            pEntry->dev = hDevice;

            if (port_mask & (0x01 << port)) {

                /* We are modifying the IPV6 SRC address to include the egress port */
                uint8_t bm_egress_port = (0x01 << port) << port_offset;
                add_egress_port(pEntry->pBufDesc->pBuf, bm_egress_port);

                pcapTxPacket(pEntry->pBufDesc->pBuf, pEntry->pBufDesc->trxSize);

                adin2111_main_queue_add(&txQueue, (adin2111_Port_e) port, pEntry->pBufDesc, adin2111_tx_cb);
            }
        } else {
            retv = ERR_MEM;
            break;
        }
    }

    if(retv == ERR_OK) {
        /* Give semaphore after 1 or more buffers have been put onto the queue */
        xSemaphoreGive(tx_ready_sem);
    }

    return retv;
}

/**
 * @brief Enables the ADIN2111 interface
 *
 * @note Zephyr specific function (called by OS after defining net device)
 *
 * @param *dev Zephyr device type
 * @return int 0 on success
 */
int adin2111_hw_start(adin2111_DeviceHandle_t* dev) {
    return adin2111_Enable(*dev);
}

/**
 * @brief Disables the ADIN2111 interface
 *
 * @note Zephyr specific function (called by OS after defining net device)
 *
 * @param *dev Zephyr device type
 * @return int 0 on success
 */
int adin2111_hw_stop(adin2111_DeviceHandle_t* dev) {
    return adin2111_Disable(*dev);
}
