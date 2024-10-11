#include "bm_l2.h"
#include "bm_config.h"
#include "eth_adin2111.h"
#include "lwip/ethip6.h"
#include "lwip/prot/ethernet.h"
#include "lwip/snmp.h"
#include "task_priorities.h"
extern "C" {
#include "bm_ip.h"
#include "bm_os.h"
#include "util.h"
}

//TODO: this will have to be moved as we go along
extern struct netif netif;

#define ethernet_packet_size_byte 14
#define ipv6_version_traffic_class_flow_label_size_bytes 4
#define ipv6_payload_length_size_bytes 2
#define ipv6_next_header_size_bytes 1
#define ipv6_hop_limit_size_bytes 1
#define ipv6_source_address_size_bytes 16
#define ipv6_destination_address_size_bytes 16

#define ipv6_source_address_offset                                                             \
  (ethernet_packet_size_byte + ipv6_version_traffic_class_flow_label_size_bytes +              \
   ipv6_payload_length_size_bytes + ipv6_next_header_size_bytes + ipv6_hop_limit_size_bytes)
#define ipv6_destination_address_offset                                                        \
  (ipv6_source_address_offset + ipv6_source_address_size_bytes)

#define ADD_EGRESS_PORT(addr, port) (addr[ipv6_source_address_offset + EGRESS_PORT_IDX] = port)
#define ADD_INGRESS_PORT(addr, port)                                                           \
  (addr[ipv6_source_address_offset + INGRESS_PORT_IDX] = port)
#define IS_GLOBAL_MULTICAST(addr)                                                              \
  (((uint8_t *)(addr))[ipv6_destination_address_offset] == 0xFFU &&                            \
   ((uint8_t *)(addr))[ipv6_destination_address_offset + 1] == 0x03U)

#define evt_queue_len (32)

typedef int (*bm_l2_dev_powerdwn_cb_t)(const void *devHandle, bool on, uint8_t port_mask);

typedef struct {
  uint8_t num_ports;
  uint8_t enabled_ports_mask;
  void *device_handle;
  uint8_t start_port_idx;
  bm_netdev_type_t type;
  bm_l2_dev_powerdwn_cb_t dev_pwr_func;
} bm_netdev_ctx_t;

typedef enum {
  BM_L2_TX,
  BM_L2_RX,
  BM_L2_LINK_UP,
  BM_L2_LINK_DOWN,
  BM_L2_SET_NETIF_UP,
  BM_L2_SET_NETIF_DOWN,
} bm_l2_queue_type_e;

typedef struct {
  void *device_handle;
  uint8_t port_mask;
  void *buf;
  bm_l2_queue_type_e type;
  uint32_t length;
} l2_queue_element_t;

typedef struct {
  bm_netdev_ctx_t devices[BM_NETDEV_COUNT];
  bm_l2_link_change_cb_t link_change_cb;

  uint8_t available_ports_mask;
  uint8_t available_port_mask_idx;
  uint8_t enabled_port_mask;
  BmQueue evt_queue;
} bm_l2_ctx_t;

static bm_l2_ctx_t CTX;

/* TODO: ADIN2111-specifc, let's move to ADIN driver.
   Rx Callback can only get the MAC Handle, not the Device handle itself */
static int32_t bm_l2_get_device_index(const void *device_handle) {
  int32_t rval = -1;

  for (int32_t idx = 0; idx < BM_NETDEV_TYPE_MAX; idx++) {
    if (device_handle == ((adin2111_DeviceHandle_t)CTX.devices[idx].device_handle)) {
      rval = idx;
    }
  }

  return rval;
}

/*!
  @brief L2 TX Function

  @details Queues buffer to be sent over the network

  @param *buf buffer with frame/data to send out
  @param port_mask port(s) to transmit message over

  @return BmOK if successful
  @return BmErr if unsuccessful
*/
static BmErr bm_l2_tx(void *buf, uint32_t length, uint8_t port_mask) {
  BmErr err = BmOK;

  // device_handle not needed for tx
  // Don't send to ports that are offline
  l2_queue_element_t tx_evt = {NULL, port_mask & CTX.enabled_port_mask, buf, BM_L2_TX, length};

  bm_l2_tx_prep(buf, length);
  if (bm_queue_send(CTX.evt_queue, &tx_evt, 10) != BmOK) {
    bm_l2_free(buf);
    err = BmENOMEM;
  }

  return err;
}

/*!
  @brief L2 RX Function - called by low level driver when new data is available

  @param device_handle device handle
  @param payload buffer with received data
  @param payload_len buffer length
  @param port_mask which port was this received over

  @return BmOK if successful
  @return BmErr if unsuccsessful
*/
static BmErr bm_l2_rx(void *device_handle, uint8_t *payload, uint16_t payload_len,
                      uint8_t port_mask) {
  BmErr err = BmOK;

  l2_queue_element_t tx_evt = {device_handle, port_mask, NULL, BM_L2_RX, payload_len};

  do {
    tx_evt.buf = bm_l2_new(payload_len);
    if (tx_evt.buf == NULL) {
      printf("No mem for buf in RX pathway\n");
      err = BmENOMEM;
      break;
    }
    memcpy(bm_l2_get_payload(tx_evt.buf), payload, payload_len);

    if (bm_queue_send(CTX.evt_queue, (void *)&tx_evt, 0) != BmOK) {
      bm_l2_free(tx_evt.buf);
      err = BmENOMEM;
      break;
    }
  } while (0);
  return err;
}

/*!
  @brief Process TX Event

  @details Receive message from L2 queue and send over all
           network interfaces (if there are multiple), the specific port
           to use is stored in the tx_event data structure

  @param *tx_evt tx event with buffer, port, and other information
*/
static void bm_l2_process_tx_evt(l2_queue_element_t *tx_evt) {
  uint8_t mask_idx = 0;

  if (tx_evt) {
    for (uint32_t idx = 0; idx < BM_NETDEV_TYPE_MAX; idx++) {
      switch (CTX.devices[idx].type) {
      case BM_NETDEV_TYPE_ADIN2111: {
        err_t retv = adin2111_tx((adin2111_DeviceHandle_t)CTX.devices[idx].device_handle,
                                 (uint8_t *)bm_l2_get_payload(tx_evt->buf), tx_evt->length,
                                 (tx_evt->port_mask >> mask_idx) & ADIN2111_PORT_MASK,
                                 CTX.devices[idx].start_port_idx);
        mask_idx += CTX.devices[idx].num_ports;
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
        break;
      }
      }
    }
    bm_l2_free(tx_evt->buf);
  }
}

/*!
  @brief Process RX event

  @details Receive message from L2 queue and:
             1. re-transmit over other port if the message is multicast
             2. send up to lwip for processing via net_if->input()

  \param *rx_evt - rx event with buffer, port, and other information
  \return none
*/
static void bm_l2_process_rx_evt(l2_queue_element_t *rx_evt) {
  if (rx_evt) {
    uint8_t rx_port_mask = 0;
    uint8_t device_idx = bm_l2_get_device_index(rx_evt->device_handle);
    switch (CTX.devices[device_idx].type) {
    case BM_NETDEV_TYPE_ADIN2111:
      rx_port_mask =
          ((rx_evt->port_mask & ADIN2111_PORT_MASK) << CTX.devices[device_idx].start_port_idx);
      break;
    case BM_NETDEV_TYPE_NONE:
    default:
      /* No device or not supported. How did we get here? */
      rx_port_mask = 0;
      break;
    }

    /* We need to code the RX Port into the IPV6 address passed to lwip */
    ADD_INGRESS_PORT(((uint8_t *)bm_l2_get_payload(rx_evt->buf)), rx_port_mask);

    if (IS_GLOBAL_MULTICAST(bm_l2_get_payload(rx_evt->buf))) {
      uint8_t new_port_mask = CTX.available_ports_mask & ~(rx_port_mask);
      bm_l2_tx(rx_evt->buf, rx_evt->length, new_port_mask);
    }

    /* TODO: This is the place where routing and filtering functions would happen, to prevent passing the
       packet to net_if->input() if unnecessary, as well as forwarding routed multicast data to interested
       neighbors and user devices. */

    // Submit packet to ip stack.
    // Upper level RX Callback is responsible for freeing the packet
    bm_l2_submit(rx_evt->buf, rx_evt->length);
  }
}

/*!
  @brief Link change callback function called from eth driver
         whenever the state of a link changes

  @param *device_handle eth driver handle
  @param port (device specific) port that changed
  @param state up/down
*/
static void link_change_cb(void *device_handle, uint8_t port, bool state) {

  l2_queue_element_t link_change_evt = {device_handle, port, NULL, BM_L2_LINK_DOWN, 0};
  if (state) {
    link_change_evt.type = BM_L2_LINK_UP;
  }

  if (device_handle) {
    // Schedule link change event
    bm_queue_send(CTX.evt_queue, &link_change_evt, 10);
  }
}

/*!
  @brief Handle link change event

  @param *device_handle eth driver handle
  @param port (device specific) port that changed
  @param state up/down

  @return none
*/
static void handle_link_change(const void *device_handle, uint8_t port, bool state) {
  if (device_handle) {
    for (uint8_t device = 0; device < BM_NETDEV_COUNT; device++) {
      if (device_handle == CTX.devices[device].device_handle) {
        // Get the overall port number
        uint8_t port_idx = CTX.devices[device].start_port_idx + port;
        // Keep track of what ports are enabled/disabled so we don't try and send messages
        // out through them
        uint8_t port_mask = 1 << (port_idx);
        if (state) {
          CTX.enabled_port_mask |= port_mask;
        } else {
          CTX.enabled_port_mask &= ~port_mask;
        }

        if (CTX.link_change_cb) {
          CTX.link_change_cb(port_idx, state);
        } else {
          printf("port%u %s\n", port_idx, state ? "up" : "down");
        }
      }
    }
  }
}

/*!
  @brief Handle netif set event

  @param device_handle eth driver handle
  @param on true to turn the interface on, false to turn the interface off.
*/
static void bm_l2_process_netif_evt(const void *device_handle, bool on) {
  for (uint8_t device = 0; device < BM_NETDEV_COUNT; device++) {
    if (device_handle == CTX.devices[device].device_handle) {
      if (CTX.devices[device].dev_pwr_func) {
        if (CTX.devices[device].dev_pwr_func((void *)device_handle, on,
                                             CTX.devices[device].enabled_ports_mask) == 0) {
          printf("Powered device %d : %s\n", device, (on) ? "on" : "off");
        } else {
          printf("Failed to power device %d : %s\n", device, (on) ? "on" : "off");
        }
      }
    }
  }
}

/*!
  @brief L2 thread which handles both tx and rx events

  @param *parameters - unused
*/
static void bm_l2_thread(void *parameters) {
  (void)parameters;

  while (true) {
    l2_queue_element_t event;
    if (bm_queue_receive(CTX.evt_queue, &event, UINT32_MAX) == BmOK) {
      switch (event.type) {
      case BM_L2_TX: {
        bm_l2_process_tx_evt(&event);
        break;
      }
      case BM_L2_RX: {
        bm_l2_process_rx_evt(&event);
        break;
      }
      case BM_L2_LINK_UP: {
        handle_link_change(event.device_handle, event.port_mask, true);
        break;
      }
      case BM_L2_LINK_DOWN: {
        handle_link_change(event.device_handle, event.port_mask, false);
        break;
      }
      case BM_L2_SET_NETIF_UP: {
        bm_l2_process_netif_evt(event.device_handle, true);
        bm_l2_set_netif(true);
        break;
      }
      case BM_L2_SET_NETIF_DOWN: {
        bm_l2_set_netif(false);
        bm_l2_process_netif_evt(event.device_handle, false);
        break;
      }
      default: {
      }
      }
    }
  }
}

/*!
  @brief Initialize L2 layer

  @param cb link change callback that will be called when the ethernet driver
            changes the link status

  @return BmOK if successful
  @return BmErr if unsuccessful
 */
BmErr bm_l2_init(bm_l2_link_change_cb_t cb) {
  BmErr err = BmENODEV;

  CTX.link_change_cb = cb;

  /* Reset context variables */
  CTX.available_ports_mask = 0;
  CTX.available_port_mask_idx = 0;

  for (uint32_t idx = 0; idx < BM_NETDEV_COUNT; idx++) {
    CTX.devices[idx].type = bm_netdev_config[idx].type;
    switch (CTX.devices[idx].type) {
    case BM_NETDEV_TYPE_ADIN2111: {
      configASSERT(bm_netdev_config[idx].config);
      adin2111_config_t *adin_cfg = (adin2111_config_t *)bm_netdev_config[idx].config;
      configASSERT(adin_cfg->dev);
      if (adin2111_hw_init(adin_cfg->dev, bm_l2_rx, link_change_cb, adin_cfg->port_mask) ==
          ADI_ETH_SUCCESS) {
        CTX.devices[idx].device_handle = adin_cfg->dev;
        CTX.devices[idx].num_ports = ADIN2111_PORT_NUM;
        CTX.devices[idx].start_port_idx = CTX.available_port_mask_idx;
        CTX.devices[idx].dev_pwr_func = adin2111_power_cb;
        CTX.devices[idx].enabled_ports_mask = adin_cfg->port_mask;
        CTX.available_ports_mask |= (ADIN2111_PORT_MASK << CTX.available_port_mask_idx);
        CTX.available_port_mask_idx += CTX.devices[idx].num_ports;
      } else {
        printf("Failed to init ADIN2111");
      }
      break;
    }
    case BM_NETDEV_TYPE_NONE:
    default:
      /* No device or not supported */
      CTX.devices[idx].device_handle = NULL;
      CTX.devices[idx].num_ports = 0;
      break;
    }
  }

  CTX.evt_queue = bm_queue_create(evt_queue_len, sizeof(l2_queue_element_t));
  err = bm_task_create(bm_l2_thread, "L2 TX Thread", 2048, NULL, BM_L2_TX_TASK_PRIORITY, NULL);

  return err;
}

/*!
  @brief bm_l2_tx wrapper for network stack

  @param *netif lwip network interface
  @param *buf buf with data to send out

  @return BmOK if successful
  @return BMErr if unsuccessful
*/
BmErr bm_l2_link_output(void *buf, uint32_t length) {
  // by default, send to all ports
  uint8_t port_mask = CTX.available_ports_mask;

  // if the application set an egress port, send only to that port
  constexpr size_t bcmp_egress_port_offset_in_dest_addr = 13;
  const size_t egress_port_idx = sizeof(struct eth_hdr) + offsetof(struct ip6_hdr, dest) +
                                 bcmp_egress_port_offset_in_dest_addr;
  uint8_t *eth_frame = (uint8_t *)bm_l2_get_payload(buf);
  uint8_t egress_port = eth_frame[egress_port_idx];
  constexpr uint8_t num_adin_ports = 2; // TODO generalize
  if (egress_port > 0 && egress_port <= num_adin_ports) {
    port_mask = 1 << (egress_port - 1);
  }

  // clear the egress port set by the application
  eth_frame[egress_port_idx] = 0;

  return bm_l2_tx(buf, length, port_mask);
}

/*!
  @brief Get the total number of ports

  \return number of ports
*/
uint8_t bm_l2_get_num_ports(void) { return BM_NETDEV_COUNT; }

/*!
  @brief Get the raw device handle for a specific port

  @param dev_idx port index to get the handle from
  @param *device_handle pointer to variable to store device handle in
  @param *type pointer to variable to store the device type
  @param *start_port_idx pointer to variable to store the start port for this device

  @return true if successful
  @return false if unsuccessful
*/
bool bm_l2_get_device_handle(uint8_t dev_idx, void **device_handle, bm_netdev_type_t *type,
                             uint32_t *start_port_idx) {
  bool rval = false;
  if (device_handle && type && start_port_idx) {
    do {
      if (dev_idx >= BM_NETDEV_COUNT) {
        break;
      }

      *type = CTX.devices[dev_idx].type;
      *device_handle = CTX.devices[dev_idx].device_handle;
      *start_port_idx = CTX.devices[dev_idx].start_port_idx;
      rval = true;
    } while (0);
  }

  return rval;
}

/*!
  @brief Obtains The State Of The Provided Port

  @param port port to obtain the state of

  @return true if port is online
  @retunr false if port is offline
 */
bool bm_l2_get_port_state(uint8_t port) { return (bool)(CTX.enabled_port_mask & (1 << port)); }

/*!
  @brief Public api to set a particular network device on/off

  \param device_handle eth driver handle
  \param on - true to turn the interface on, false to turn the interface off.

  \return none
*/
void bm_l2_netif_set_power(void *dev, bool on) {
  l2_queue_element_t pwr_evt = {dev, 0, NULL, BM_L2_SET_NETIF_DOWN, 0};
  if (dev) {
    if (on) {
      pwr_evt.type = BM_L2_SET_NETIF_UP;
    }

    // Schedule netif pwr event
    bm_queue_send(CTX.evt_queue, &pwr_evt, 10);
  }
}
