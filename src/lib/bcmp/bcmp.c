#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"

#include "bcmp.h"
#include "debug.h"

#include "bm_util.h"
#include "task_priorities.h"

#include "lwip/icmp.h"
#include "lwip/inet_chksum.h"
#include "lwip/raw.h"

#define BCMP_EVT_QUEUE_LEN 32

// Send heartbeats every 10 seconds (and check for expired links)
#define BCMP_HEARTBEAT_S 10

typedef struct {
  struct netif* netif;
  struct raw_pcb *pcb;
  QueueHandle_t rx_queue;
  TimerHandle_t heartbeat_timer;
} bcmpContext_t;

typedef enum {
    BCMP_EVT_RX,
    BCMP_EVT_HEARTBEAT,
} bcmp_queue_type_e;

typedef struct {
  bcmp_queue_type_e type;

  struct pbuf* pbuf;
  ip_addr_t src;

  // Used for non tx/rx items
  void *args;

} bcmp_queue_item_t;

static bcmpContext_t _ctx;

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
    bcmp_send_heartbeat();
    configASSERT(xTimerStart(_ctx.heartbeat_timer, 10));
  }
}

/*!
  BCMP packet processing function

  \param *pbuf pbuf with packet
  \param *src packet source
  \return 0 if processed ok, nonzero otherwise
*/
int32_t bmcp_process_packet(struct pbuf *pbuf, ip_addr_t *src) {
  int32_t rval = 0;
  // uint8_t src_port;
  uint8_t dst_port;

  configASSERT(pbuf);
  configASSERT(src);

   // Ingress and Egress ports are mapped to the 5th and 6th byte of the IPv6 src address
   // as per the bristlemouth protocol spec */
  // src_port = ((src->addr[1]) & 0xFF);
  dst_port = ((src->addr[1] >> 8) & 0xFF);

  CLEAR_PORTS(src->addr);


  do {
    bcmpHeader_t *header = (bcmpHeader_t *)pbuf->payload;

    uint16_t checksum = ip6_chksum_pseudo( pbuf,
                                          IP_PROTO_BCMP,
                                          pbuf->len,
                                          src, &multicast_ll_addr);

    // Valid checksum will come out to zero, since the actual checksum
    // is included and cancels out
    if(checksum) {
      printf("BCMP - Invalid checmsum\n");

      rval = -1;
      break;
    }

    // TODO - check for address match (only relevant on certain messages)
    // // Check for both link-local and unicast address match
    // if(ip6_addr_eq(header->dest, netif_ip_addr6(_ctx.netif, 0)) ||
    //    ip6_addr_eq(header->dest, netif_ip_addr6(_ctx.netif, 1))) {

    // }

    // bcmpHeader_t *header = (bcmpHeader_t *)pbuf->payload;
    switch(header->type) {
      case BCMP_HEARTBEAT: {
        bcmp_process_heartbeat(header->payload, src, dst_port);
        break;
      }

      case BCMP_DFU_START:
      case BCMP_DFU_PAYLOAD_REQ:
      case BCMP_DFU_PAYLOAD:
      case BCMP_DFU_END:
      case BCMP_DFU_ACK:
      case BCMP_DFU_HEARTBEAT:
      case BCMP_DFU_BEGIN_HOST:
        // TODO: PUMP DFU STATE MACHINE.
        break;
      default: {
        printf("Unsupported BMCP message %04X\n", header->type);
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

  bcmp_queue_item_t item = {BCMP_EVT_HEARTBEAT, NULL, {{0,0,0,0}, 0}, NULL};

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
    if (pbuf->tot_len < (PBUF_IP_HLEN + sizeof(bcmpHeader_t))) {
      break;
    }

    if(pbuf_remove_header(pbuf, PBUF_IP_HLEN) != 0) {
      //  restore original packet
      pbuf_add_header(pbuf, PBUF_IP_HLEN);
      break;
    }

    // Make a copy of the IP address since we'll be modifying it later when we
    // remove the src/dest ports (and since it might not be in the pbuf so someone
    // else is managing that memory)
    bcmp_queue_item_t item = {BCMP_EVT_RX, pbuf, *src, NULL};

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
        bmcp_process_packet(item.pbuf, &item.src);
        break;
      }

      case BCMP_EVT_HEARTBEAT: {
        bcmp_send_heartbeat();
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
err_t bcmp_tx(const ip_addr_t *dst, bcmpMessaegType_t type, uint8_t *buff, uint16_t len) {
    struct pbuf *pbuf;

    configASSERT((uint32_t)len + sizeof(bcmpHeader_t) < UINT16_MAX);

    pbuf = pbuf_alloc(PBUF_IP, len + sizeof(bcmpHeader_t), PBUF_RAM);
    configASSERT(pbuf);

    bcmpHeader_t *header = (bcmpHeader_t *)pbuf->payload;
    header->type = (uint16_t)type;
    header->checksum = 0;
    header->flags = 0; // Unused for now
    header->rsvd = 0; // Unused for now

    memcpy(header->payload, buff, len);

    const ip_addr_t *src_ip = netif_ip_addr6(_ctx.netif, 0);

    header->checksum = ip6_chksum_pseudo( pbuf,
                                          IP_PROTO_BCMP,
                                          len + sizeof(bcmpHeader_t),
                                          src_ip, dst);

    err_t rval = raw_sendto_if_src( _ctx.pcb,
                                    pbuf,
                                    dst,
                                    _ctx.netif,
                                    src_ip); // Using link-local address

    if(rval != ERR_OK) {
      pbuf_free(pbuf);
      printf("Error sending BMCP packet %d\n", rval);
    }

    return rval;
}

/*!
  BCMP initialization

  \param *netif lwip network interface to use
  \return none
*/
void bcmp_init(struct netif* netif) {
  _ctx.netif = netif;
  _ctx.pcb = raw_new(IP_PROTO_BCMP);
  configASSERT(_ctx.pcb);


  /* Create threads and Queues */
  _ctx.rx_queue = xQueueCreate(BCMP_EVT_QUEUE_LEN, sizeof(bcmp_queue_item_t));
  configASSERT(_ctx.rx_queue);

  BaseType_t rval = xTaskCreate(bcmp_thread,
                     "BCMP",
                     1024,
                     NULL,
                     BCMP_TASK_PRIORITY,
                     NULL);
  configASSERT(rval == pdPASS);
}