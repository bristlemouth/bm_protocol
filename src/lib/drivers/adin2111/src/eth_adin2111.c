#include <errno.h>
#include <string.h>

#include "adi_bsp.h"
#include "eth_adin2111.h"

// Includes for FreeRTOS
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"

#include "netif/ethernet.h"
#include "lwip/ethip6.h"
#include "lwip/pbuf.h"
#include "lwip/api.h"
#include "lwip/debug.h"
#include "lwip/snmp.h"
#include "lwip/stats.h"
#include "lwip/nd6.h"

static TaskHandle_t serviceTask = NULL;
static uint8_t dev_mem[ADIN2111_DEVICE_SIZE];

static adin2111_DriverConfig_t drvConfig = {
    .pDevMem = (void *)dev_mem,
    .devMemSize = sizeof(dev_mem),
    .fcsCheckEn = false,
};

queue_t txQueue;
queue_t rxQueue;

DMA_BUFFER_ALIGN(static uint8_t txQueueBuf[QUEUE_NUM_ENTRIES][MAX_FRAME_BUF_SIZE], 4);
DMA_BUFFER_ALIGN(static uint8_t rxQueueBuf[QUEUE_NUM_ENTRIES][MAX_FRAME_BUF_SIZE], 4);
static adi_eth_BufDesc_t txBufDesc[QUEUE_NUM_ENTRIES];
static adi_eth_BufDesc_t rxBufDesc[QUEUE_NUM_ENTRIES];

static void adin2111_main_queue_init(queue_t *pQueue, adi_eth_BufDesc_t bufDesc[QUEUE_NUM_ENTRIES], uint8_t buf[QUEUE_NUM_ENTRIES][MAX_FRAME_BUF_SIZE]);
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
static QueueHandle_t                rx_port_queue, tx_ready_sem;

#define RX_PORT_QUEUE_LENGTH        (4)
#define BINARY_SEMAPHORE_LENGTH	    (1)
#define IFNAME0                     'e'
#define IFNAME1                     '0'
#define ETHERNET_MTU                (1500)
#define NETIF_LINK_SPEED_IN_BPS     10000000

static adin2111_DeviceStruct_t      dev;
static adin2111_DeviceHandle_t      hDevice = &dev;
static struct netif*                eth_if;

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
static void adin2111_main_queue_init(queue_t *pQueue, adi_eth_BufDesc_t bufDesc[QUEUE_NUM_ENTRIES],
                                      uint8_t buf[QUEUE_NUM_ENTRIES][MAX_FRAME_BUF_SIZE])
{
    pQueue->head = 0;
    pQueue->tail = 0;
    pQueue->full = false;
    for (uint32_t i = 0; i < QUEUE_NUM_ENTRIES; i++) {
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
    (void) pCBParam;

    adi_eth_BufDesc_t *pBufDesc;
    adin2111_Port_e rx_port;

    pBufDesc = (adi_eth_BufDesc_t *)pArg;
    rx_port = (adin2111_Port_e)pBufDesc->port;

    xQueueSend(rx_port_queue, ( void * ) &rx_port, 0);
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
    adin2111_Port_e _rx_port;

    /* Create the queue set large enough to hold an event for every space in
    every queue and semaphore that is to be added to the set. */
    queue_set = xQueueCreateSet( RX_PORT_QUEUE_LENGTH + BINARY_SEMAPHORE_LENGTH );

    /* Create the queues and semaphores that will be contained in the set. */
    rx_port_queue = xQueueCreate( RX_PORT_QUEUE_LENGTH, sizeof(adin2111_Port_e));

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
            if (!adin2111_main_queue_is_empty(&txQueue)) {
                pEntry = adin2111_main_queue_tail(&txQueue);
                if ((pEntry != NULL) && (!pEntry->sent)) {
                    pEntry->sent = true;
                    result = adin2111_SubmitTxBuffer(hDevice, (adin2111_TxPort_e)pEntry->port, pEntry->pBufDesc);
                    if (result == ADI_ETH_SUCCESS) {
                        printf("Submitted TX Buf to ADIN2111\n");
                        adin2111_main_queue_remove(&txQueue);
                    }
                }
            }
        } else if (activated_member == rx_port_queue) {
            xQueueReceive(activated_member, &_rx_port, 0);
            if (!adin2111_main_queue_is_empty(&rxQueue)) {
                pEntry = adin2111_main_queue_tail(&rxQueue);

                if( !pEntry ) {
                    //LOG_ERR( "Shouldn't happen: Null adin queue entry" );
                    configASSERT(0);
                    goto out;
                }

                struct pbuf* p = pbuf_alloc(PBUF_RAW, MAX_FRAME_BUF_SIZE, PBUF_RAM);
                if (p == NULL) {
                    printf("No mem for pbuf in RX pathway\n");
                    // LOG_ERR("No mem for pbuf allocation");
                    // configASSERT(0);
                    goto out;
                }

                // Copy over RX packet contents to a pBuf to submit to L2
                memcpy(((uint8_t*) p->payload) , pEntry->pBufDesc->pBuf, pEntry->pBufDesc->trxSize);

                /* Submit packet to the L2 Layer */
                if (eth_if->input(p, eth_if) != ERR_OK) {
                    LWIP_DEBUGF(NETIF_DEBUG, ("IP input error\r\n"));
                    pbuf_free(p);
                    p = NULL;
                }
out:
                /* Put the buffer back into queue and re-submit to the ADIN2111 driver */
                adin2111_main_queue_remove(&rxQueue);
                adin2111_main_queue_add(&rxQueue, _rx_port, pEntry->pBufDesc, adin2111_rx_cb);
                result = adin2111_SubmitRxBuffer(hDevice, pEntry->pBufDesc);
                if (result != ADI_ETH_SUCCESS) {
                    configASSERT(0);
                }
            }
        }
    }
}

static void adin2111_hw_init(struct netif *netif) {
    adi_eth_Result_e result;
    adi_mac_AddressRule_t addrRule;
    BaseType_t rval;

    printf("Initializing ADIN2111\n");

    /* Initialize BSP to kickoff thread to service GPIO and SPI DMA interrupts */
    if (adi_bsp_init()) {
        // LOG_ERR("BSP Init failed");
        configASSERT(0);
    }

    /* ADIN2111 Init process */
    result = adin2111_Init(hDevice, &drvConfig);
    if (result != ADI_ETH_SUCCESS) {
        // LOG_ERR("ADIN2111 failed initialization");
        configASSERT(0);
    }

    /* generate random MAC addresses for each port/phy */
    addrRule.VALUE16 = 0x0000;
    addrRule.APPLY2PORT1 = 1;
    addrRule.APPLY2PORT2 = 1;
    addrRule.TO_HOST = 1;

    result = adin2111_AddAddressFilter(hDevice, netif->hwaddr, NULL, addrRule);
    if (result != ADI_ETH_SUCCESS) {
        // LOG_ERR("ADIN2111 failed to add address filter");
        configASSERT(0);
    }

    /* Register Callback for link change */
    result = adin2111_RegisterCallback(hDevice, adin2111_link_change_cb,
                       ADI_MAC_EVT_LINK_CHANGE);
    if (result != ADI_ETH_SUCCESS) {
        // LOG_ERR("ADIN2111 failed to register Link Change callback");
        configASSERT(0);
        
    }

    /* Prepare Rx buffers */
    adin2111_main_queue_init(&txQueue, txBufDesc, txQueueBuf);
    adin2111_main_queue_init(&rxQueue, rxBufDesc, rxQueueBuf);
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
        // LOG_ERR("ADIN2111 failed Sync Config");
        configASSERT(0);
    }

    rval = xTaskCreate(adin2111_service_thread,
                       "ADIN2111 Service Thread",
                       8192,
                       NULL,
                       15,
                       &serviceTask);
    configASSERT(rval == pdTRUE);

    /* This used to be started by zephyr after networking layer got set up, should we enable the ADIN here? */
    adin2111_hw_start(&hDevice);
}

static err_t adin2111_tx(struct netif *netif, struct pbuf *p) {
    (void) netif;
    queue_entry_t *pEntry;
    err_t retv = ERR_OK;

    if (!adin2111_main_queue_is_full(&txQueue)) {
        pEntry = adin2111_main_queue_head(&txQueue);
		pEntry->pBufDesc->trxSize = p->len;
		memcpy(pEntry->pBufDesc->pBuf, p->payload, pEntry->pBufDesc->trxSize);

        /* FIXME: Send out of PORT1 for now */
        adin2111_main_queue_add(&txQueue, ADIN2111_PORT_1, pEntry->pBufDesc, adin2111_tx_cb);
        xSemaphoreGive(tx_ready_sem);
    } else {
        retv = ERR_MEM;
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

err_t eth_adin2111_init(struct netif *netif) {

    /* Initialize the ADIN Driver */
    adin2111_hw_init(netif);

    MIB2_INIT_NETIF(netif, snmp_ifType_ethernet_csmacd, NETIF_LINK_SPEED_IN_BPS);

    netif->name[0] = IFNAME0;
    netif->name[1] = IFNAME1;
    netif->output_ip6 = ethip6_output;
    netif->linkoutput = adin2111_tx;

    /* Store netif in local pointer */
    eth_if = netif;

    netif->mtu = ETHERNET_MTU;
    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP;

    return ERR_OK;
}