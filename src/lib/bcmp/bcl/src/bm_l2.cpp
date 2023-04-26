#include "FreeRTOS.h"
#include "bm_config.h"
#include "bm_l2.h"
#include "eth_adin2111.h"
#include "lwip/ethip6.h"
#include "lwip/prot/ethernet.h"
#include "lwip/snmp.h"
#include "task_priorities.h"

#define IFNAME0                     'b'
#define IFNAME1                     'm'
#define NETIF_LINK_SPEED_BPS        10000000
#define ETHERNET_MTU                1500

#define EVT_QUEUE_LEN (32)

typedef struct {
    uint8_t num_ports;
    void* device_handle;
    uint8_t start_port_idx;
    bm_netdev_type_t type;
} bm_netdev_ctx_t;

typedef enum {
    BM_L2_TX,
    BM_L2_RX,
    BM_L2_LINK_UP,
    BM_L2_LINK_DOWN,
} bm_l2_queue_type_e;

typedef struct {
    void* device_handle;
    uint8_t port_mask;
    struct pbuf* pbuf;
    bm_l2_queue_type_e type;
} l2_queue_element_t;

typedef struct {
    struct netif* net_if;
    bm_netdev_ctx_t devices[BM_NETDEV_COUNT];

    /* TODO: We are only supporting the ADIN2111 net device. Currently assuming we have up to BM_NETDEV_TYPE_MAX ADIN2111s.
       We want to eventually support new net devices and pre-allocate here before giving to init function */
    adin2111_DeviceStruct_t adin_devices[BM_NETDEV_TYPE_MAX];

    bm_l2_link_change_cb_t link_change_cb;

    uint8_t adin_device_counter;
    uint8_t available_ports_mask;
    uint8_t available_port_mask_idx;
    uint8_t enabled_port_mask;
    QueueHandle_t evt_queue;
} bm_l2_ctx_t;

static bm_l2_ctx_t bm_l2_ctx;

/* TODO: ADIN2111-specifc, let's move to ADIN driver.
   Rx Callback can only get the MAC Handle, not the Device handle itself */
static int32_t bm_l2_get_device_index(const void* device_handle) {
    int32_t rval = -1;

    for (int32_t idx=0; idx<BM_NETDEV_TYPE_MAX; idx++) {
        if (device_handle == ((adin2111_DeviceHandle_t) bm_l2_ctx.devices[idx].device_handle)){
            rval = idx;
        }
    }

    return rval;
}

/*!
  Process TX event. Receive message from L2 queue and send over all
  network interfaces (if there are multiple). The specific port
  to use is stored in the tx_event data structure

  \param *tx_evt - tx event with buffer, port, and other information
  \return none
*/
static void bm_l2_process_tx_evt(l2_queue_element_t *tx_evt) {
    configASSERT(tx_evt);

    uint8_t mask_idx = 0;

    for (uint32_t idx=0; idx < BM_NETDEV_TYPE_MAX; idx++) {
        switch (bm_l2_ctx.devices[idx].type) {
            case BM_NETDEV_TYPE_ADIN2111: {
                err_t retv = adin2111_tx((adin2111_DeviceHandle_t) bm_l2_ctx.devices[idx].device_handle, static_cast<uint8_t *>(tx_evt->pbuf->payload), tx_evt->pbuf->len,
                                   (tx_evt->port_mask >> mask_idx) & ADIN2111_PORT_MASK, bm_l2_ctx.devices[idx].start_port_idx);
                mask_idx += bm_l2_ctx.devices[idx].num_ports;
                if (retv != ERR_OK) {
                    printf("Failed to submit TX buffer to ADIN\n");
                }
                break;
            }
            case BM_NETDEV_TYPE_NONE: {
                /* No device */
                break;
            }
            default: {
                /* Unsupported device */
                configASSERT(0);
                break;
            }
        }
    }
    pbuf_free(tx_evt->pbuf);
}

/*!
  Process RX event. Receive message from L2 queue and:
  1. re-transmit over other port if the message is multicast
  2. send up to lwip for processing via net_if->input()

  \param *rx_evt - rx event with buffer, port, and other information
  \return none
*/
static void bm_l2_process_rx_evt(l2_queue_element_t *rx_evt) {
    configASSERT(rx_evt);

    uint8_t rx_port_mask = 0;
    uint8_t device_idx = bm_l2_get_device_index(rx_evt->device_handle);
    switch (bm_l2_ctx.devices[device_idx].type) {
        case BM_NETDEV_TYPE_ADIN2111:
            rx_port_mask = ((rx_evt->port_mask & ADIN2111_PORT_MASK) << bm_l2_ctx.devices[device_idx].start_port_idx);
            break;
        case BM_NETDEV_TYPE_NONE:
        default:
            /* No device or not supported. How did we get here? */
            configASSERT(0);
            rx_port_mask = 0;
            break;
    }

    /* We need to code the RX Port into the IPV6 address passed to lwip */
    ADD_INGRESS_PORT((static_cast<uint8_t *>(rx_evt->pbuf->payload)), rx_port_mask);

    if (IS_GLOBAL_MULTICAST(rx_evt->pbuf->payload)) {
        uint8_t new_port_mask = bm_l2_ctx.available_ports_mask & ~(rx_port_mask);
        bm_l2_tx(rx_evt->pbuf, new_port_mask);
    }

    /* TODO: This is the place where routing and filtering functions would happen, to prevent passing the
       packet to net_if->input() if unnecessary, as well as forwarding routed multicast data to interested
       neighbors and user devices. */

    // Submit packet to lwip. User RX Callback is responsible for freeing the packet
    // We're using tcpip_input in the netif, which is thread safe, so no
    // need for additional locking
    if (bm_l2_ctx.net_if->input(rx_evt->pbuf, bm_l2_ctx.net_if) != ERR_OK) {
        pbuf_free(rx_evt->pbuf);
    }
}

/*!
  Link change callback function called from eth driver
  whenever the state of a link changes.

  \param *device_handle eth driver handle
  \param port (device specific) port that changed
  \param state up/down

  \return none
*/
static void _link_change_cb(void* device_handle, uint8_t port, bool state) {
    configASSERT(device_handle);

    l2_queue_element_t link_change_evt = {device_handle, port, NULL, BM_L2_LINK_DOWN};
    if(state) {
        link_change_evt.type = BM_L2_LINK_UP;
    }

    // Schedule link change event
    configASSERT(xQueueSend(bm_l2_ctx.evt_queue, &link_change_evt, 10) == pdTRUE);
}

/*!
  Handle link change event

  \param *device_handle eth driver handle
  \param port (device specific) port that changed
  \param state up/down

  \return none
*/
static void _handle_link_change(const void *device_handle, uint8_t port, bool state) {
    configASSERT(device_handle);

    for(uint8_t device = 0; device < BM_NETDEV_COUNT; device++) {
        if(device_handle == bm_l2_ctx.devices[device].device_handle) {
            // Get the overall port number
            uint8_t port_idx = bm_l2_ctx.devices[device].start_port_idx + port;
            // Keep track of what ports are enabled/disabled so we don't try and send messages
            // out through them
            uint8_t port_mask = 1 << (port_idx);
            if (state){
                bm_l2_ctx.enabled_port_mask |= port_mask;
            } else {
                bm_l2_ctx.enabled_port_mask &= ~port_mask;
            }

            if(bm_l2_ctx.link_change_cb) {
                bm_l2_ctx.link_change_cb(port_idx, state);
            } else {
                printf("port%u %s\n",
                port_idx,
                state ? "up" : "down");
            }
        }
    }
}

/*!
  L2 thread which handles both tx and rx events

  \param *parameters - unused
  \return none
*/
static void bm_l2_thread(void *parameters) {
    (void)parameters;

    while(true) {
        l2_queue_element_t event;
        configASSERT(xQueueReceive(bm_l2_ctx.evt_queue, &event, portMAX_DELAY));

        switch(event.type) {
            case BM_L2_TX: {
                bm_l2_process_tx_evt(&event);
                break;
            }
            case BM_L2_RX: {
                bm_l2_process_rx_evt(&event);
                break;
            }
            case BM_L2_LINK_UP: {
                _handle_link_change(event.device_handle, event.port_mask, true);
                break;
            }
            case BM_L2_LINK_DOWN: {
                _handle_link_change(event.device_handle, event.port_mask, false);
                break;
            }
            default: {
                configASSERT(0);
            }
        }

    }
}

/*!
  L2 TX Function. Queues buffer to be sent over the network

  \param *pbuf buffer with frame/data to send out
  \param port_mask port(s) to transmit message over
  \return ERR_OK if successful, something else otherwise
*/
err_t bm_l2_tx(struct pbuf *pbuf, uint8_t port_mask) {
    err_t retv = ERR_OK;

    // device_handle not needed for tx
    // Don't send to ports that are offline
    l2_queue_element_t tx_evt = {NULL, port_mask & bm_l2_ctx.enabled_port_mask, pbuf, BM_L2_TX};

    pbuf_ref(pbuf);
    if(xQueueSend(bm_l2_ctx.evt_queue, &tx_evt, 10) != pdTRUE) {
        pbuf_free(pbuf);
        retv = ERR_MEM;
    }

    return retv;
}

/*!
  L2 RX Function - called by low level driver when new data is available

  \param device_handle device handle
  \param payload buffer with received data
  \param payload_len buffer length
  \param port_mask which port was this received over
  \return ERR_OK if successful, something else otherwise
*/
err_t bm_l2_rx(void* device_handle, uint8_t* payload, uint16_t payload_len, uint8_t port_mask) {
    err_t retv = ERR_OK;

    l2_queue_element_t tx_evt = {device_handle, port_mask, NULL, BM_L2_RX};

    do {
        tx_evt.pbuf = pbuf_alloc(PBUF_RAW, payload_len, PBUF_RAM);
        if (tx_evt.pbuf == NULL) {
            printf("No mem for pbuf in RX pathway\n");
            retv = ERR_MEM;
            break;
        }
        tx_evt.pbuf->len = payload_len;
        memcpy(tx_evt.pbuf->payload, payload, payload_len);

        if(xQueueSend(bm_l2_ctx.evt_queue, (void *) &tx_evt, 0) != pdTRUE) {
            pbuf_free(tx_evt.pbuf);
            retv = ERR_MEM;
            break;
        }
    } while (0);
    return retv;
}

/*!
  bm_l2_tx wrapper for lwip's network interface

  \param *netif lwip network interface
  \param *pbuf pbuf with data to send out
  \return ERR_OK if successful, something else otherwise
*/
err_t bm_l2_link_output(struct netif *netif, struct pbuf *pbuf) {
    (void) netif;

    /* Send on all available ports (Multicast) */
    return bm_l2_tx(pbuf, bm_l2_ctx.available_ports_mask);
}

// Netif initialization for BM devices
err_t bm_l2_netif_init(struct netif *netif) {
    MIB2_INIT_NETIF(netif, snmp_ifType_ethernet_csmacd, NETIF_LINK_SPEED_BPS);

    netif->name[0] = IFNAME0;
    netif->name[1] = IFNAME1;
    netif->output_ip6 = ethip6_output;
    netif->linkoutput = bm_l2_link_output;

    /* Store netif in local pointer */
    bm_l2_ctx.net_if = netif;

    netif->mtu = ETHERNET_MTU;
    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP;

    return ERR_OK;
}

/* L2 Initialization */
err_t bm_l2_init(bm_l2_link_change_cb_t link_change_cb) {
    err_t retv = ERR_OK;

    bm_l2_ctx.link_change_cb = link_change_cb;

    /* Reset context variables */
    bm_l2_ctx.adin_device_counter = 0;
    bm_l2_ctx.available_ports_mask = 0;
    bm_l2_ctx.available_port_mask_idx = 0;

    for (uint32_t idx=0; idx < BM_NETDEV_COUNT; idx++) {
        bm_l2_ctx.devices[idx].type = bm_netdev_config[idx].type;
        switch (bm_l2_ctx.devices[idx].type) {
            case BM_NETDEV_TYPE_ADIN2111:
                if (adin2111_hw_init(&bm_l2_ctx.adin_devices[bm_l2_ctx.adin_device_counter], bm_l2_rx, _link_change_cb) == ADI_ETH_SUCCESS) {
                    bm_l2_ctx.devices[idx].device_handle = &bm_l2_ctx.adin_devices[bm_l2_ctx.adin_device_counter++];
                    bm_l2_ctx.devices[idx].num_ports = ADIN2111_PORT_NUM;
                    bm_l2_ctx.devices[idx].start_port_idx = bm_l2_ctx.available_port_mask_idx;
                    bm_l2_ctx.available_ports_mask |= (ADIN2111_PORT_MASK << bm_l2_ctx.available_port_mask_idx);
                    bm_l2_ctx.available_port_mask_idx += ADIN2111_PORT_NUM;
                } else {
                    printf("Failed to init ADIN2111");
                }
                break;
            case BM_NETDEV_TYPE_NONE:
            default:
                /* No device or not supported */
                bm_l2_ctx.devices[idx].device_handle = NULL;
                bm_l2_ctx.devices[idx].num_ports = 0;
                break;
        }
    }

    bm_l2_ctx.evt_queue = xQueueCreate( EVT_QUEUE_LEN, sizeof(l2_queue_element_t));

    BaseType_t rval = xTaskCreate(bm_l2_thread,
                       "L2 TX Thread",
                       2048,
                       NULL,
                       BM_L2_TX_TASK_PRIORITY,
                       NULL);
    configASSERT(rval == pdPASS);

    return retv;
}

/*!
  Get the total number of ports

  \return number of ports
*/
uint8_t bm_l2_get_num_ports() {
    return BM_NETDEV_COUNT;
}

/*!
  Get the raw device handle for a specific port

  \param dev_idx - port index to get the handle from
  \param *device_handle - pointer to variable to store device handle in
  \param *type - pointer to variable to store the device type
  \param *start_port_idx - pointer to variable to store the start port for this device
  \return true if successful, false otherwise
*/
bool bm_l2_get_device_handle(uint8_t dev_idx, void **device_handle, bm_netdev_type_t *type, uint32_t *start_port_idx) {
    configASSERT(device_handle);
    configASSERT(type);
    configASSERT(start_port_idx);
    bool rval = false;
    do {
        if(dev_idx >= BM_NETDEV_COUNT) {
            break;
        }

        *type = bm_l2_ctx.devices[dev_idx].type;
        // adin2111_DeviceHandle_t handle = device_handle;
        *device_handle = bm_l2_ctx.devices[dev_idx].device_handle;
        *start_port_idx = bm_l2_ctx.devices[dev_idx].start_port_idx;
        rval = true;
    } while(0);

    return rval;
}

bool bm_l2_get_port_state(uint8_t port) {
    return (bool)(bm_l2_ctx.enabled_port_mask & (1 << port));
}
