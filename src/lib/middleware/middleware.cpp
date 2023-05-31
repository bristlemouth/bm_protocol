#include <string.h>
#include "FreeRTOS.h"
#include "bm_ports.h"
#include "bm_pubsub.h"
#include "bm_util.h"
#include "lwip/inet.h"
#include "lwip/ip_addr.h"
#include "lwip/pbuf.h"
#include "lwip/udp.h"
#include "middleware.h"
#include "safe_udp.h"
#include "semphr.h"
#include "task_priorities.h"

#define NET_QUEUE_LEN 64

typedef struct {
    struct netif* netif;
    struct udp_pcb* pcb;
    uint16_t port;
    xQueueHandle netQueue;
} middlewareContext_t;

typedef struct {
  struct pbuf *pbuf;
  ip6_addr_t addr;
} netQueueItem_t;

static middlewareContext_t _ctx;
static void middleware_net_task( void *parameters );
static void middleware_net_rx_cb(void *arg, struct udp_pcb *upcb, struct pbuf *buf, const ip_addr_t *addr, u16_t port);

/*!
  Process received CLI commands from iridium. Split them up, if multiple,
  and send to CLI processor.
  \param[in] *netif - lwip network interface
  \param[in] port - UDP port to use for middleware
  \return None
*/
void bm_middleware_init(struct netif* netif, uint16_t port) {
  configASSERT(netif);
  configASSERT(port != 0);

  BaseType_t rval;

  _ctx.netif = netif;
  _ctx.port = port;

  _ctx.netQueue = xQueueCreate(NET_QUEUE_LEN, sizeof(netQueueItem_t));
  configASSERT(_ctx.netQueue);

  rval = xTaskCreate(
              middleware_net_task,
              "mwNet",
              // TODO - verify stack size
              configMINIMAL_STACK_SIZE * 4,
              NULL,
              MIDDLEWARE_NET_TASK_PRIORITY,
              NULL);

  configASSERT(rval == pdTRUE);
}

/*!
  Middleware network transmit function
  \param[in] *pbuf - data to send over UDP
  \return 0 if OK nonzero otherwise (see udp_send for error codes)
*/
int32_t middleware_net_tx(struct pbuf *pbuf) {
  // TODO - Do we always send global multicast or link local?
  return safe_udp_sendto_if(_ctx.pcb, pbuf, &multicast_global_addr, _ctx.port, _ctx.netif);
}

/*!
  Middleware UDP rx callback
  \param[in] *arg - unused
  \param[in] *pcb - UDP PCB for middleware
  \param[in] *buf - data pbuf
  \param[in] *addr - Source address
  \param[in] port -
  \return None
*/
// cppcheck-suppress constParameter
static void middleware_net_rx_cb(void *arg, struct udp_pcb *upcb, struct pbuf *buf,
                 const ip_addr_t *addr, u16_t port) {

  (void)arg;
  (void)port;
  // sanity check that we're processing middleware data
  configASSERT(upcb == _ctx.pcb);

  do {
    netQueueItem_t queueItem;

    if (buf) {
      // Copy ipv6 address to queueItem
      memcpy(&queueItem.addr, addr, sizeof(queueItem.addr));

      queueItem.pbuf = buf;

      //
      if(xQueueSend(_ctx.netQueue, &queueItem, 0) != pdTRUE) {
        printf("Error sending to Queue\n");
        // buf will be freed below
        break;
      }

      // Clear buf so we don't free it below
      buf = NULL;
    } else {
      printf("Error. Message has no payload buf\n");
    }
  } while(0);

  // Free buf if we haven't used it (and cleared it)
  if(buf) {
    pbuf_free(buf);
  }
}

/*!
  Publish data to local device (self)
  \param[in] *pbuf - pbuf with pub data
  \return None
*/
int32_t bm_middleware_local_pub(struct pbuf *pbuf) {
  int32_t rval = 0;

  configASSERT(pbuf);

  netQueueItem_t queueItem;
  queueItem.pbuf = pbuf;

  memcpy(&queueItem.addr, netif_ip6_addr(_ctx.netif, 1), sizeof(queueItem.addr));

  // add one to reference count since we'll be using it in two places
  pbuf_ref(pbuf);
  if(xQueueSend(_ctx.netQueue, &queueItem, 0) != pdTRUE) {
      printf("Error sending to Queue\n");

      // JK, don't retain it since we didn't send it out
      pbuf_free(pbuf);
      rval = -1;
  }

  return rval;
}

/*!
  Middleware network processing task. Will receive middleware packets in queue from
  middleware_net_rx_cb and process them.
  \param[in] *parameters - unused
  \return None
*/
static void middleware_net_task( void *parameters ) {
  // Don't warn about unused parameters
  (void) parameters;

  _ctx.pcb = udp_new_ip_type(IPADDR_TYPE_V6);
  udp_bind(_ctx.pcb, IP_ANY_TYPE, _ctx.port);
  udp_recv(_ctx.pcb, middleware_net_rx_cb, NULL);

  for(;;) {
    netQueueItem_t item;

    BaseType_t rval = xQueueReceive(_ctx.netQueue, &item, portMAX_DELAY);
    configASSERT(rval == pdTRUE);
    configASSERT(item.pbuf);

    //
    // Do stuff with item here
    //    printf("Received %u bytes from %s\n", item.pbuf->len, );

    bm_handle_msg(ip_to_nodeid(&item.addr), item.pbuf);

    // Free item
    if(item.pbuf) {
      pbuf_free(item.pbuf);
    }
  }
}
