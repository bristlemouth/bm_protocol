#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "timers.h"

#include "bcmp.h"
#include "debug.h"

#include "bm_util.h"
#include "task_priorities.h"

#include "lwip/icmp.h"
#include "lwip/inet_chksum.h"
#include "lwip/raw.h"
#include "bcmp_heartbeat.h"
#include "bcmp_info.h"
#include "bcmp_neighbors.h"
#include "bcmp_ping.h"
#include "bcmp_config.h"
#include "bcmp_time.h"
#include "bcmp_topology.h"
#include "bcmp_resource_discovery.h"
#include "timer_callback_handler.h"

#include "bm_dfu.h"

#define BCMP_EVT_QUEUE_LEN 32

// Send heartbeats every 10 seconds (and check for expired links)
#define BCMP_HEARTBEAT_S 10

// 1500 MTU minus ipv6 header
#define MAX_PAYLOAD_LEN (1500 - sizeof(struct ip6_hdr))

static constexpr uint32_t DEFAULT_MESSAGE_TIMEOUT_MS = 1000;
static constexpr uint32_t MESSAGE_TIMER_EXPIRY_PERIOD_MS = 500;

typedef struct bcmp_request_element {
  uint32_t seq_num;
  uint16_t type;
  uint32_t send_timestamp_ms;
  uint32_t timeout_ms;
  bcmp_reply_message_cb callback;
  bcmp_request_element *next;
} bcmp_request_element_t;

typedef struct {
  struct netif* netif;
  struct raw_pcb *pcb;
  QueueHandle_t rx_queue;
  TimerHandle_t heartbeat_timer;
  // Linked list of sent BCMP messages
  bcmp_request_element *messages_list;
  SemaphoreHandle_t messages_list_mutex;
  TimerHandle_t messages_expiration_timer;
  uint32_t message_count;
} bcmpContext_t;

typedef enum {
    BCMP_EVT_RX,
    BCMP_EVT_HEARTBEAT,
} bcmp_queue_type_e;

typedef struct {
  bcmp_queue_type_e type;

  struct pbuf* pbuf;
  ip_addr_t src;
  ip_addr_t dst;

  // Used for non tx/rx items
  void *args;

} bcmp_queue_item_t;

static bcmpContext_t _ctx;

static bool _message_list_add_message(bcmp_request_element *message);
static bool _message_list_remove_message(bcmp_request_element *message);
static bcmp_request_element *_message_list_create_element(uint16_t seq_num, uint16_t type, uint32_t timeout_ms, bcmp_reply_message_cb callback);
static void _message_list_timer_callback(TimerHandle_t tmr);
static bcmp_request_element *_message_list_find_message(uint16_t seq_num);
static void _message_list_timer_expiry_cb(void *arg);
static bool _message_is_sequenced_reply(uint16_t type);
static bool _message_is_sequenced_request(uint16_t type);

/*!
  BCMP link change event callback

  \param port - system port in which the link change occurred
  \param state - 0 for down 1 for up
  \return none
*/
void bcmp_link_change(uint8_t port, bool state) {
  (void)port; // Not using the port for now
  if(state) {
    // Send heartbeat since we just connected to someone and (re)start the
    // heartbeat timer
    bcmp_send_heartbeat(BCMP_HEARTBEAT_S);
    configASSERT(xTimerStart(_ctx.heartbeat_timer, 10));
  }
}

/*!
  Process a DFU message. Allocates memory that the consumer is in charge of freeing.
  \param pbuf[in] pbuf buffer
  \return none
*/
static void dfu_copy_and_process_message(struct pbuf *pbuf) {
  configASSERT(pbuf);
  bcmp_header_t *header = static_cast<bcmp_header_t *>(pbuf->payload);
  uint8_t* buf = static_cast<uint8_t *>(pvPortMalloc((pbuf->len) - sizeof(bcmp_header_t)));
  configASSERT(buf);
  memcpy(buf, header->payload, (pbuf->len) - sizeof(bcmp_header_t));
  bm_dfu_process_message(buf, (pbuf->len) - sizeof(bcmp_header_t));
}

/*!
  BCMP packet processing function

  \param *pbuf pbuf with packet
  \param *src packet source
  \param *dst packet destination
  \return 0 if processed ok, nonzero otherwise
*/
int32_t bcmp_process_packet(struct pbuf *pbuf, ip_addr_t *src, ip_addr_t *dst) {
  int32_t rval = 0;
  // uint8_t egress_port;
  uint8_t ingress_port;

  configASSERT(pbuf);
  configASSERT(src);

  // Ingress and Egress ports are mapped to the 5th and 6th byte of the IPv6 src address.
  // This is in conflict with the v1.0.0, June 2023 version of the Bristlemouth spec.
  // TODO: align with spec
  // egress_port = ((src->addr[1]) & 0xFF);
  ingress_port = ((src->addr[1] >> 8) & 0xFF);

  CLEAR_PORTS(src->addr);


  do {
    bcmp_header_t *header = static_cast<bcmp_header_t *>(pbuf->payload);

    uint16_t checksum = ip6_chksum_pseudo(pbuf,
                                          IP_PROTO_BCMP,
                                          pbuf->len,
                                          src, dst);

    // Valid checksum will come out to zero, since the actual checksum
    // is included and cancels out
    if(checksum) {
      printf("BCMP - Invalid checksum\n");

      rval = -1;
      break;
    }

    bcmp_reply_message_cb cb = NULL;

    // Check if this message is a reply to a message we sent
    if (_message_is_sequenced_reply(header->type) && !_message_is_sequenced_request(header->type)) {
      bcmp_request_element *sent_message = _message_list_find_message(header->seq_num);
      if (sent_message) {
        printf("BCMP - Received reply to our request message with seq_num %d\n", header->seq_num);
        if (xSemaphoreTake(_ctx.messages_list_mutex, pdMS_TO_TICKS(DEFAULT_MESSAGE_TIMEOUT_MS)) == pdPASS) {
          cb = sent_message->callback;
          _message_list_remove_message(sent_message);
          xSemaphoreGive(_ctx.messages_list_mutex);
        }
      }
    }

    switch(header->type) {
      case BCMP_HEARTBEAT: {
        // Send out heartbeats
        bcmp_process_heartbeat(reinterpret_cast<bcmp_heartbeat_t *>(header->payload), src, ingress_port);
        break;
      }

      case BCMP_DEVICE_INFO_REQUEST: {
        bcmp_process_info_request(reinterpret_cast<bcmp_device_info_request_t *>(header->payload), src, dst);
        break;
      }

      case BCMP_DEVICE_INFO_REPLY: {
        bcmp_process_info_reply(reinterpret_cast<bcmp_device_info_reply_t *>(header->payload));
        break;
      }

      case BCMP_ECHO_REQUEST: {
        bcmp_process_ping_request(reinterpret_cast<bcmp_echo_request_t *>(header->payload), src, dst, header->seq_num);
        break;
      }

      case BCMP_ECHO_REPLY: {
        bcmp_process_ping_reply(reinterpret_cast<bcmp_echo_reply_t *>(header->payload));
        break;
      }

      case BCMP_RESOURCE_TABLE_REQUEST: {
        bcmp_resource_discovery::bcmp_process_resource_discovery_request(reinterpret_cast<bcmp_resource_table_request_t*>(header->payload), dst);
        break;
      }

      case BCMP_RESOURCE_TABLE_REPLY: {
        bcmp_resource_discovery::bcmp_process_resource_discovery_reply(reinterpret_cast<bcmp_resource_table_reply_t*>(header->payload), ip_to_nodeid(src));
        break;
      }

      case BCMP_SYSTEM_TIME_REQUEST:
      case BCMP_SYSTEM_TIME_RESPONSE:
      case BCMP_SYSTEM_TIME_SET: {
        bool should_forward = bcmp_time_process_time_message(
            static_cast<bcmp_message_type_t>(header->type), header->payload);
        if (should_forward) {
          // Forward the message to all ports other than the ingress port.
          bcmp_ll_forward(pbuf, ingress_port);
        }
        break;
      }

      case BCMP_CONFIG_GET:
      case BCMP_CONFIG_SET:
      case BCMP_CONFIG_VALUE:
      case BCMP_CONFIG_COMMIT:
      case BCMP_CONFIG_STATUS_REQUEST:
      case BCMP_CONFIG_STATUS_RESPONSE:
      case BCMP_CONFIG_DELETE_REQUEST:
      case BCMP_CONFIG_DELETE_RESPONSE: {
        if (cb) {
          cb(header->payload);
        } else {
          bool should_forward = bcmp_process_config_message(
              static_cast<bcmp_message_type_t>(header->type), header->payload, header->seq_num);
          if (should_forward) {
            // Forward the message to all ports other than the ingress port.
            bcmp_ll_forward(pbuf, ingress_port);
          }
        }
        break;
      }

      case BCMP_DFU_START:
      case BCMP_DFU_PAYLOAD_REQ:
      case BCMP_DFU_PAYLOAD:
      case BCMP_DFU_END:
      case BCMP_DFU_ACK:
      case BCMP_DFU_ABORT:
      case BCMP_DFU_HEARTBEAT:
      case BCMP_DFU_REBOOT_REQ:
      case BCMP_DFU_REBOOT:
      case BCMP_DFU_BOOT_COMPLETE:
      {
        dfu_copy_and_process_message(pbuf);
        break;
      }

      case BCMP_NEIGHBOR_TABLE_REQUEST: {
        bcmp_process_neighbor_table_request(reinterpret_cast<bcmp_neighbor_table_request_t *>(header->payload), src, dst);
        break;
      }

      case BCMP_NEIGHBOR_TABLE_REPLY: {
        bcmp_process_neighbor_table_reply(reinterpret_cast<bcmp_neighbor_table_reply_t *>(header->payload));
        break;
      }

      default: {
        printf("Unsupported BCMP message %04X\n", header->type);
        rval = -1;
        break;
      }
    }

  } while(0);

  return rval;
}


/*!
  FreeRTOS timer handler for sending out heartbeats. No work is done in the timer
  handler, but instead an event is queued up to be handled in the BCMP task.

  \param tmr unused
  \return none
*/
static void heartbeat_timer_handler(TimerHandle_t tmr){
  (void) tmr;

  bcmp_queue_item_t item = {BCMP_EVT_HEARTBEAT, NULL, {{0,0,0,0}, 0}, {{0,0,0,0}, 0}, NULL};

  configASSERT(xQueueSend(_ctx.rx_queue, &item, 0) == pdTRUE);
}

/*!
  lwip raw recv callback for BCMP packets. Called from lwip task.
  Packet is added to main BCMP queue for processing

  \param *arg unused
  \param *pcb unused
  \param *pbuf packet buffer
  \param *src packet sender address
  \return 1 if the packet was consumed, 0 otherwise (someone else will take it)
*/
static uint8_t bcmp_recv(void *arg, struct raw_pcb *pcb, struct pbuf *pbuf, const ip_addr_t *src) {
  (void)arg;
  (void)pcb;

  // don't eat the packet unless we process it
  uint8_t rval = 0;
  configASSERT(pbuf);

  do {
    if (pbuf->tot_len < (PBUF_IP_HLEN + sizeof(bcmp_header_t))) {
      break;
    }

    // Grab a pointer to the ip6 header so we can get the packet destination
    struct ip6_hdr *ip6_hdr = static_cast<struct ip6_hdr *>(pbuf->payload);

    if(pbuf_remove_header(pbuf, PBUF_IP_HLEN) != 0) {
      //  restore original packet
      pbuf_add_header(pbuf, PBUF_IP_HLEN);
      break;
    }

    // Make a copy of the IP address since we'll be modifying it later when we
    // remove the src/dest ports (and since it might not be in the pbuf so someone
    // else is managing that memory)
    bcmp_queue_item_t item = {BCMP_EVT_RX, pbuf, *src, {{0,0,0,0}, 0}, NULL};

    // Copy the destination into the queue item
    memcpy(item.dst.addr, ip6_hdr->dest.addr, sizeof(item.dst.addr));

    if(xQueueSend(_ctx.rx_queue, &item, 0) != pdTRUE) {
      printf("Error sending to Queue\n");
      pbuf_free(pbuf);
    }

    // eat the packet
    rval = 1;
  } while (0);

  return rval;
}


/*!
  BCMP task. All BCMP events are handled here.

  \param parameters unused
  \return none
*/
static void bcmp_thread(void *parameters) {
  (void) parameters;

  // Start listening for BCMP packets
  raw_recv(_ctx.pcb, bcmp_recv, NULL);
  configASSERT(raw_bind(_ctx.pcb, IP_ADDR_ANY) == ERR_OK);

  _ctx.heartbeat_timer = xTimerCreate("bcmp_heartbeat", pdMS_TO_TICKS(BCMP_HEARTBEAT_S * 1000),
                                 pdTRUE, NULL, heartbeat_timer_handler);
  configASSERT(_ctx.heartbeat_timer);

  // TODO - send out heartbeats on link change

  for(;;) {
    bcmp_queue_item_t item;

    BaseType_t rval = xQueueReceive(_ctx.rx_queue, &item, portMAX_DELAY);
    configASSERT(rval == pdTRUE);

    switch(item.type) {
      case BCMP_EVT_RX: {
        bcmp_process_packet(item.pbuf, &item.src, &item.dst);
        break;
      }

      case BCMP_EVT_HEARTBEAT: {
        // Should we check neighbors on a differnt timer?
        // Check neighbor status to see if any dropped
        bcmp_check_neighbors();

        // Send out heartbeats
        bcmp_send_heartbeat(BCMP_HEARTBEAT_S);
        break;
      }

      default: {
        break;
      }
    }

    if(item.pbuf) {
      pbuf_free(item.pbuf);
    }
  }

}

/*!
  BCMP packet transmit function. Header and checksum added and computer within

  \param *dst destination ip
  \param type message type
  \param *buff message buffer
  \param len message length
  \return ERR_OK on success, something else otherwise
*/
err_t bcmp_tx(const ip_addr_t *dst, bcmp_message_type_t type, uint8_t *buff, uint16_t len, uint16_t seq_num, bcmp_reply_message_cb reply_cb) {
  err_t rval;

  do {
    if((uint32_t)len + sizeof(bcmp_header_t) > MAX_PAYLOAD_LEN) {
      // Payload too big, don't try to transmit.
      rval = ERR_VAL;
      break;
    }

    struct pbuf *pbuf = pbuf_alloc(PBUF_IP, len + sizeof(bcmp_header_t), PBUF_RAM);
    configASSERT(pbuf);

    bcmp_header_t *header = static_cast<bcmp_header_t *>(pbuf->payload);
    header->type = (uint16_t)type;
    header->checksum = 0;
    header->flags = 0; // Unused for now
    header->rsvd = 0; // Unused for now
    if (_message_is_sequenced_reply(type)) {
      // if we are replying to a message, use the sequence number from the received message
      header->seq_num = seq_num;
    } else if (_message_is_sequenced_request(type)) {
      // If we are sending a new request, use our own sequence number
      header->seq_num = _ctx.message_count;
      _ctx.message_count++;
      bcmp_request_element *sent_message = _message_list_create_element(header->seq_num, header->type, DEFAULT_MESSAGE_TIMEOUT_MS, reply_cb);
      _message_list_add_message(sent_message);
      printf("BCMP - Sending message with seq_num %d\n", header->seq_num);
    } else {
      // If the message doesn't use sequence numbers, set it to 0
      header->seq_num = 0;
    }
    header->frag_total = 0; // Unused for now
    header->frag_id = 0; // Unused for now
    header->next_header = 0; // Unused for now

    memcpy(header->payload, buff, len);

    const ip_addr_t *src_ip = netif_ip_addr6(_ctx.netif, 0);

    header->checksum = ip6_chksum_pseudo( pbuf,
                                          IP_PROTO_BCMP,
                                          len + sizeof(bcmp_header_t),
                                          src_ip, dst);

    rval = raw_sendto_if_src( _ctx.pcb,
                              pbuf,
                              dst,
                              _ctx.netif,
                              src_ip); // Using link-local address

    // We're done with this pbuf
    // raw_sendto_if_src eventually calls bm_l2_tx, which does a pbuf_ref
    // on this buffer.
    pbuf_free(pbuf);

    if(rval != ERR_OK) {
      printf("Error sending BMCP packet %d\n", rval);
    }
  } while(0);

  return rval;
}

/*! \brief Forward the payload to all ports other than the ingress port.

    See section 5.4.4.2 of the Bristlemouth spec for details.

    \param[in] pbuf Packet buffer to forward.
    \param[in] ingress_port Port on which the packet was received.
    \return ERR_OK on success, or an error that occurred.
*/
err_t bcmp_ll_forward(struct pbuf *pbuf, uint8_t ingress_port) {
  ip_addr_t port_specific_dst;
  ip6_addr_set(&port_specific_dst, &multicast_ll_addr);

  // TODO: Make more generic. This is specifically for a 2-port device.
  uint8_t egress_port = ingress_port == 1 ? 2 : 1;
  port_specific_dst.addr[3] = 0x1000000 | (egress_port << 8);

  const ip_addr_t *src = netif_ip_addr6(_ctx.netif, 0);

  // L2 will clear the egress port from the destination address,
  // so calculate the checksum on the link-local multicast address.
  bcmp_header_t *header = static_cast<bcmp_header_t *>(pbuf->payload);
  header->checksum = 0;
  header->checksum = ip6_chksum_pseudo(pbuf, IP_PROTO_BCMP, pbuf->len, src, &multicast_ll_addr);

  err_t rval = raw_sendto_if_src(_ctx.pcb, pbuf, &port_specific_dst, _ctx.netif, src);
  if (rval != ERR_OK) {
    printf("Error forwarding BMCP packet link-locally: %d\n", rval);
  }
  return rval;
}

/*!
  BCMP DFU packet transmit function.

  \param type[in] - bcmp dfu message type
  \param buff[in] - message payload
  \param len[in] - message length
  \return true if succesful false otherwise.
*/
static bool bcmp_dfu_tx (bcmp_message_type_t type, uint8_t *buff, uint16_t len) {
  configASSERT(type >= BCMP_DFU_START && type <= BCMP_DFU_LAST_MESSAGE);
  return (bcmp_tx(&multicast_global_addr,type,buff,len) == ERR_OK);
}

/*!
  BCMP initialization

  \param *netif lwip network interface to use
  \return none
*/
void bcmp_init(struct netif* netif, NvmPartition * dfu_partition, Configuration* user_cfg, Configuration* sys_cfg){
  _ctx.netif = netif;
  _ctx.pcb = raw_new(IP_PROTO_BCMP);
  configASSERT(_ctx.pcb);

  _ctx.message_count = 0;

  /* Create threads and Queues */
  _ctx.rx_queue = xQueueCreate(BCMP_EVT_QUEUE_LEN, sizeof(bcmp_queue_item_t));
  configASSERT(_ctx.rx_queue);
  _ctx.messages_list_mutex = xSemaphoreCreateMutex();
  configASSERT(_ctx.messages_list_mutex);
  _ctx.messages_expiration_timer = xTimerCreate("bcmp_message_expiration", pdMS_TO_TICKS(MESSAGE_TIMER_EXPIRY_PERIOD_MS),
                                 pdTRUE, NULL, _message_list_timer_callback);
  configASSERT(xTimerStart(_ctx.messages_expiration_timer, 10) == pdPASS);

  bm_dfu_init(bcmp_dfu_tx, dfu_partition);
  bcmp_config_init(user_cfg, sys_cfg);
  bcmp_resource_discovery::bcmp_resource_discovery_init();

  BaseType_t rval = xTaskCreate(bcmp_thread,
                     "BCMP",
                     1024,
                     NULL,
                     BCMP_TASK_PRIORITY,
                     NULL);
  configASSERT(rval == pdPASS);
}


static bool _message_list_add_message(bcmp_request_element *message) {
  bool rval = false;
  configASSERT(message);
  if (xSemaphoreTake(_ctx.messages_list_mutex, pdMS_TO_TICKS(DEFAULT_MESSAGE_TIMEOUT_MS)) == pdPASS) {
    if (_ctx.messages_list == NULL) {
      _ctx.messages_list = message;
    } else {
      bcmp_request_element *current = _ctx.messages_list;
      while (current->next != NULL) {
        current = current->next;
      }
      current->next = message;
    }
    rval = true;
    xSemaphoreGive(_ctx.messages_list_mutex);
  }
  return rval;
}

// NOTE: This function is not thread safe and can only be called within a section
// protected by the messages_list_mutex semaphore.
static bool _message_list_remove_message(bcmp_request_element *message) {
  bool rval = false;
  configASSERT(message);
  bcmp_request_element *element_to_delete = NULL;
  if (_ctx.messages_list == message) {
    element_to_delete = _ctx.messages_list;
    _ctx.messages_list = _ctx.messages_list->next;
  } else {
    bcmp_request_element *current = _ctx.messages_list;
    while (current && current->next != message) {
      current = current->next;
    }
    if (current) {
      element_to_delete = current->next;
      current->next = current->next->next;
    }
  }
  if (element_to_delete) {
    vPortFree(element_to_delete);
    rval = true;
  }
  return rval;
}

static bcmp_request_element *_message_list_create_element(uint16_t seq_num, uint16_t type, uint32_t timeout_ms, bcmp_reply_message_cb callback) {
  bcmp_request_element *element = static_cast<bcmp_request_element *>(pvPortMalloc(sizeof(bcmp_request_element)));
  configASSERT(element);
  element->seq_num = seq_num;
  element->type = type;
  element->send_timestamp_ms = pdTICKS_TO_MS(xTaskGetTickCount());
  element->timeout_ms = timeout_ms;
  element->callback = callback;
  element->next = NULL;
  return element;
}

static void _message_list_timer_expiry_cb(void *arg) {
  (void) arg;
  if (xSemaphoreTake(_ctx.messages_list_mutex, pdMS_TO_TICKS(DEFAULT_MESSAGE_TIMEOUT_MS)) == pdPASS) {
    bcmp_request_element *current = _ctx.messages_list;
    while (current) {
      if (pdTICKS_TO_MS(xTaskGetTickCount()) - current->send_timestamp_ms > current->timeout_ms) {
        printf("BCMP message with seq_num %d timed out\n", current->seq_num);
        _message_list_remove_message(current);
        current = _ctx.messages_list;
        continue;
      }
      current = current->next;
    }
    xSemaphoreGive(_ctx.messages_list_mutex);
  }
}

static void _message_list_timer_callback(TimerHandle_t tmr){
  (void) tmr;
  timer_callback_handler_send_cb(_message_list_timer_expiry_cb, tmr, 0);
}

static bcmp_request_element *_message_list_find_message(uint16_t seq_num) {
  bcmp_request_element *rval = NULL;
  if (xSemaphoreTake(_ctx.messages_list_mutex, pdMS_TO_TICKS(DEFAULT_MESSAGE_TIMEOUT_MS)) == pdPASS) {
    bcmp_request_element *current = _ctx.messages_list;
    while (current) {
      if (current->seq_num == seq_num) {
        printf("BCMP message with seq_num %d found\n", current->seq_num);
        rval = current;
        break;
      }
      current = current->next;
    }
    xSemaphoreGive(_ctx.messages_list_mutex);
  }
  return rval;
}

static bool _message_is_sequenced_reply(uint16_t type) {
  bool rval = false;
  if (type == BCMP_CONFIG_VALUE ||
      type == BCMP_CONFIG_STATUS_RESPONSE ||
      type == BCMP_CONFIG_DELETE_RESPONSE) {
    rval = true;
  }
  return rval;
}

static bool _message_is_sequenced_request(uint16_t type) {
  bool rval = false;
  if (type == BCMP_CONFIG_GET ||
      type == BCMP_CONFIG_SET ||
      type == BCMP_CONFIG_STATUS_REQUEST ||
      type == BCMP_CONFIG_DELETE_REQUEST) {
    rval = true;
  }
  return rval;
}