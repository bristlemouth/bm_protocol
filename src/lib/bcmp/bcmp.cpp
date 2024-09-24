#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"
#include "timers.h"
#include <string.h>

#include "bcmp.h"
#include "debug.h"

#include "bm_util.h"
#include "task_priorities.h"

#include "bcmp_config.h"
#include "bcmp_heartbeat.h"
#include "bcmp_info.h"
#include "bcmp_neighbors.h"
#include "bcmp_ping.h"
#include "bcmp_resource_discovery.h"
#include "bcmp_time.h"
#include "bcmp_topology.h"
#include "bm_os.h"
#include "lwip/inet_chksum.h"
#include "lwip/raw.h"
extern "C" {
#include "packet.h"
}
#include "timer_callback_handler.h"

#include "bm_dfu.h"

#define BCMP_EVT_QUEUE_LEN 32

// Send heartbeats every 10 seconds (and check for expired links)
#define BCMP_HEARTBEAT_S 10

// 1500 MTU minus ipv6 header
#define MAX_PAYLOAD_LEN (1500 - sizeof(struct ip6_hdr))

static constexpr uint32_t MESSAGE_TIMER_EXPIRY_PERIOD_MS = 12;

typedef struct bcmp_request_element {
  uint32_t seq_num;
  uint16_t type;
  uint32_t send_timestamp_ms;
  uint32_t timeout_ms;
  bcmp_reply_message_cb callback;
  bcmp_request_element *next;
} bcmp_request_element_t;

typedef struct {
  struct netif *netif;
  struct raw_pcb *pcb;
  QueueHandle_t rx_queue;
  TimerHandle_t heartbeat_timer;
  // Linked list of sent BCMP messages
  bcmp_request_element *messages_list;
  SemaphoreHandle_t messages_list_mutex;
  TimerHandle_t messages_expiration_timer;
  uint32_t message_count;
} bcmpContext_t;

typedef struct {
  struct pbuf *pbuf;
  const ip_addr_t *src;
  const ip_addr_t *dst;
} BcmpLwipLayout;

typedef enum {
  BCMP_EVT_RX,
  BCMP_EVT_HEARTBEAT,
} bcmp_queue_type_e;

typedef struct {
  bcmp_queue_type_e type;
  BcmpLwipLayout layout;
} bcmp_queue_item_t;

static bcmpContext_t _ctx;

void *message_get_data(void *payload) {
  BcmpLwipLayout *ret = (BcmpLwipLayout *)payload;
  return (void *)ret->pbuf->payload;
}
void *message_get_src_ip(void *payload) {
  BcmpLwipLayout *ret = (BcmpLwipLayout *)payload;
  return (void *)ret->src;
}
void *message_get_dst_ip(void *payload) {
  BcmpLwipLayout *ret = (BcmpLwipLayout *)payload;
  return (void *)ret->dst;
}
uint16_t message_get_checksum(void *payload, uint32_t size) {
  uint16_t ret = UINT16_MAX;
  BcmpLwipLayout *data = (BcmpLwipLayout *)payload;

  ret = ip6_chksum_pseudo(data->pbuf, IP_PROTO_BCMP, size,
                          (ip_addr_t *)message_get_src_ip(payload),
                          (ip_addr_t *)message_get_dst_ip(payload));

  return ret;
}

/*!
  BCMP link change event callback

  \param port - system port in which the link change occurred
  \param state - 0 for down 1 for up
  \return none
*/
void bcmp_link_change(uint8_t port, bool state) {
  (void)port; // Not using the port for now
  if (state) {
    // Send heartbeat since we just connected to someone and (re)start the
    // heartbeat timer
    bcmp_send_heartbeat(BCMP_HEARTBEAT_S);
    configASSERT(xTimerStart(_ctx.heartbeat_timer, 10));
  }
}

//TODO implemnent with updated packet module
///*!
//  Process a DFU message. Allocates memory that the consumer is in charge of freeing.
//  \param pbuf[in] pbuf buffer
//  \return none
//*/
//static void dfu_copy_and_process_message(BcmpProcessData data) {
//  bcmp_header_t *header = static_cast<bcmp_header_t *>(pbuf->payload);
//  uint8_t *buf = static_cast<uint8_t *>(pvPortMalloc((pbuf->len) - sizeof(bcmp_header_t)));
//  configASSERT(buf);
//  memcpy(buf, data.payload, (pbuf->len) - sizeof(bcmp_header_t));
//  bm_dfu_process_message(buf, (pbuf->len) - sizeof(bcmp_header_t));
//}

/*!
  FreeRTOS timer handler for sending out heartbeats. No work is done in the timer
  handler, but instead an event is queued up to be handled in the BCMP task.

  \param tmr unused
  \return none
*/
static void heartbeat_timer_handler(TimerHandle_t tmr) {
  (void)tmr;

  bcmp_queue_item_t item = {BCMP_EVT_HEARTBEAT, {NULL, NULL, NULL}};

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
static uint8_t bcmp_recv(void *arg, struct raw_pcb *pcb, struct pbuf *pbuf,
                         const ip_addr_t *src) {
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

    if (pbuf_remove_header(pbuf, PBUF_IP_HLEN) != 0) {
      //  restore original packet
      pbuf_add_header(pbuf, PBUF_IP_HLEN);
      break;
    }

    // Make a copy of the IP address since we'll be modifying it later when we
    // remove the src/dest ports (and since it might not be in the pbuf so someone
    // else is managing that memory)
    struct pbuf *p_ref = pbuf_alloc(PBUF_IP, pbuf->len, PBUF_RAM);
    ip_addr_t *src_ref = (ip_addr_t *)bm_malloc(sizeof(ip_addr_t));
    ip_addr_t *dst_ref = (ip_addr_t *)bm_malloc(sizeof(ip_addr_t));
    pbuf_copy(p_ref, pbuf);
    memcpy(dst_ref, ip6_hdr->dest.addr, sizeof(ip_addr_t));
    memcpy(src_ref, src, sizeof(ip_addr_t));

    bcmp_queue_item_t item = {BCMP_EVT_RX, {p_ref, src_ref, dst_ref}};
    if (xQueueSend(_ctx.rx_queue, &item, 0) != pdTRUE) {
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
  (void)parameters;

  // Start listening for BCMP packets
  raw_recv(_ctx.pcb, bcmp_recv, NULL);
  configASSERT(raw_bind(_ctx.pcb, IP_ADDR_ANY) == ERR_OK);

  _ctx.heartbeat_timer = xTimerCreate("bcmp_heartbeat", pdMS_TO_TICKS(BCMP_HEARTBEAT_S * 1000),
                                      pdTRUE, NULL, heartbeat_timer_handler);
  configASSERT(_ctx.heartbeat_timer);

  // TODO - send out heartbeats on link change

  for (;;) {
    bcmp_queue_item_t item;

    BaseType_t rval = xQueueReceive(_ctx.rx_queue, &item, portMAX_DELAY);
    configASSERT(rval == pdTRUE);

    switch (item.type) {
    case BCMP_EVT_RX: {
      process_received_message(&item.layout, item.layout.pbuf->len - sizeof(BcmpHeader));
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

    if (item.layout.pbuf) {
      pbuf_free(item.layout.pbuf);
    }
    if (item.layout.dst) {
      bm_free((void *)item.layout.dst);
    }
    if (item.layout.src) {
      bm_free((void *)item.layout.src);
    }
  }
}

/*!
  BCMP packet transmit function. Header and checksum added and computer within

  \param *dst destination ip
  \param type message type
  \param *buff message buffer
  \param len message length
  \param seq_num The sequence number of the message.
  \param reply_cb A callback function to handle reply messages.
  \param request_timeout_ms The timeout for the request in milliseconds.
  \return ERR_OK on success, something else otherwise
*/
BmErr bcmp_tx(const ip_addr_t *dst, BcmpMessageType type, uint8_t *buff, uint16_t len,
              uint16_t seq_num, BmErr (*reply_cb)(uint8_t *payload),
              uint32_t request_timeout_ms) {
  (void)request_timeout_ms;
  BmErr err = BmEINVAL;

  if (dst && (uint32_t)len + sizeof(BcmpHeartbeat) <= MAX_PAYLOAD_LEN) {
    struct pbuf *pbuf = pbuf_alloc(PBUF_IP, len + sizeof(BcmpHeader), PBUF_RAM);
    const ip_addr_t *src_ip = netif_ip_addr6(_ctx.netif, 0);
    BcmpLwipLayout layout = {pbuf, src_ip, dst};
    configASSERT(pbuf);

    err = serialize(&layout, buff, len, type, seq_num, reply_cb);

    if (err == BmOK) {
      // Using link-local address
      err = raw_sendto_if_src(_ctx.pcb, pbuf, dst, _ctx.netif, src_ip) == ERR_OK ? BmOK
                                                                                 : BmEBADMSG;

      // We're done with this pbuf
      // raw_sendto_if_src eventually calls bm_l2_tx, which does a pbuf_ref
      // on this buffer.
      pbuf_free(pbuf);

      if (err != BmOK) {
        printf("Error sending BMCP packet %d\n", err);
      }
    } else {
      printf("Could not properly serialize message\n");
    }
  }

  return err;
}

/*! \brief Forward the payload to all ports other than the ingress port.

    See section 5.4.4.2 of the Bristlemouth spec for details.

    \param[in] pbuf Packet buffer to forward.
    \param[in] ingress_port Port on which the packet was received.
    \return ERR_OK on success, or an error that occurred.
*/
BmErr bcmp_ll_forward(BcmpHeader *header, void *payload, uint32_t size, uint8_t ingress_port) {
  ip_addr_t port_specific_dst;
  ip6_addr_set(&port_specific_dst, &multicast_ll_addr);
  BmErr err = BmEINVAL;

  // TODO: Make more generic. This is specifically for a 2-port device.
  uint8_t egress_port = ingress_port == 1 ? 2 : 1;
  port_specific_dst.addr[3] = 0x1000000 | (egress_port << 8);

  const ip_addr_t *src = netif_ip_addr6(_ctx.netif, 0);

  // L2 will clear the egress port from the destination address,
  // so calculate the checksum on the link-local multicast address.
  struct pbuf *pbuf = pbuf_alloc(PBUF_IP, size + sizeof(bcmp_header_t), PBUF_RAM);
  header->checksum = 0;
  header->checksum = ip6_chksum_pseudo(pbuf, IP_PROTO_BCMP, pbuf->len, src, &multicast_ll_addr);
  memcpy(pbuf->payload, header, sizeof(BcmpHeader));
  memcpy((uint8_t *)pbuf->payload + sizeof(BcmpHeader), payload, size);

  err = raw_sendto_if_src(_ctx.pcb, pbuf, &port_specific_dst, _ctx.netif, src) == ERR_OK
            ? BmOK
            : BmEBADMSG;
  if (err != BmOK) {
    printf("Error forwarding BMCP packet link-locally: %d\n", err);
  }
  pbuf_free(pbuf);
  return err;
}

/*!
  BCMP DFU packet transmit function.

  \param type[in] - bcmp dfu message type
  \param buff[in] - message payload
  \param len[in] - message length
  \return true if succesful false otherwise.
*/
static bool bcmp_dfu_tx(bcmp_message_type_t type, uint8_t *buff, uint16_t len) {
  configASSERT(type >= BCMP_DFU_START && type <= BCMP_DFU_LAST_MESSAGE);
  return (bcmp_tx(&multicast_global_addr, (BcmpMessageType)type, buff, len) == BmOK);
}

/*!
  BCMP initialization

  \param *netif lwip network interface to use
  \return none
*/
void bcmp_init(struct netif *netif, NvmPartition *dfu_partition, Configuration *user_cfg,
               Configuration *sys_cfg) {
  _ctx.netif = netif;
  _ctx.pcb = raw_new(IP_PROTO_BCMP);
  configASSERT(_ctx.pcb);

  _ctx.message_count = 0;

  /* Create threads and Queues */
  _ctx.rx_queue = xQueueCreate(BCMP_EVT_QUEUE_LEN, sizeof(bcmp_queue_item_t));
  configASSERT(_ctx.rx_queue);
  _ctx.messages_list_mutex = xSemaphoreCreateMutex();
  configASSERT(_ctx.messages_list_mutex);

  heartbeat_init();
  bcmp_topology_init();
  bcmp_process_info_init();
  packet_init(message_get_src_ip, message_get_dst_ip, message_get_data, message_get_checksum);
  bm_dfu_init(bcmp_dfu_tx, dfu_partition, sys_cfg);
  bcmp_config_init(user_cfg, sys_cfg);
  bcmp_resource_discovery::bcmp_resource_discovery_init();

  BaseType_t rval = xTaskCreate(bcmp_thread, "BCMP", 1024, NULL, BCMP_TASK_PRIORITY, NULL);
  configASSERT(rval == pdPASS);
}
