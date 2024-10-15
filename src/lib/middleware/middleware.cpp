#include "middleware.h"
#include "bm_pubsub.h"
#include "bm_service.h"
extern "C" {
#include "bcmp.h"
#include "bm_ip.h"
#include "bm_os.h"
}
#include <string.h>

#define middleware_task_size 512
#define net_queue_len 64
#define udp_header_size 8
#define max_payload_len_udp (max_payload_len - udp_header_size)

typedef struct {
  void *pcb;
  uint16_t port;
  BmQueue net_queue;
} MiddlewareCtx;

typedef struct {
  void *buf;
  uint64_t node_id;
  uint32_t size;
} NetQueueItem;

static MiddlewareCtx CTX;

/*!
  @brief Middleware Receiving Callback Bound To UDP Interface

  @param buf buffer to interperet
  @param node_id node id to queue buffer for

  @return BmOK on success
  @return BmErr on failure
*/
static BmErr middleware_net_rx_cb(void *buf, uint64_t node_id, uint32_t size) {
  BmErr err = BmEINVAL;
  NetQueueItem queue_item;

  if (buf) {
    queue_item.buf = buf;
    queue_item.node_id = node_id;
    queue_item.size = size;

    if ((err = bm_queue_send(CTX.net_queue, &queue_item, 0)) != BmOK) {
      bm_udp_cleanup(buf);
    }
  } else {
    printf("Error. Message has no payload buf\n");
  }

  return err;
}

/*!
  @brief Middleware network processing task

  @details Will receive middleware packets in queue from
           middleware receive callback and process them

  @param *arg unused

  @return None
*/
static void middleware_net_task(void *arg) {
  (void)arg;
  NetQueueItem item;
  BmErr err = BmOK;

  CTX.pcb = bm_udp_bind_port(CTX.port, middleware_net_rx_cb);

  for (;;) {
    err = bm_queue_receive(CTX.net_queue, &item, UINT32_MAX);
    if (err == BmOK && item.buf) {
      bm_handle_msg(item.node_id, item.buf, item.size);
      bm_udp_cleanup(item.buf);
    }
  }
}

/*!
  @brief Initialize Middleware 

  @param port UDP port to use for middleware
*/
BmErr bm_middleware_init(uint16_t port) {
  BmErr err = BmEINVAL;

  if (port != 0) {
    err = BmENOMEM;
    CTX.port = port;
    CTX.net_queue = bm_queue_create(net_queue_len, sizeof(NetQueueItem));

    if (CTX.net_queue) {
      bm_service_init();
      err = bm_task_create(middleware_net_task, "Middleware Task",
                           // TODO - verify stack size
                           middleware_task_size, NULL, middleware_net_task_priority, NULL);
    }
  }

  return err;
}

/*!
  @brief Middleware network transmit function

  @param[in] *buf data to send over UDP
  
  @return BmOK on success
  @return BmErr on failure
*/
BmErr bm_middleware_net_tx(void *buf, uint32_t size) {
  BmErr err = BmEINVAL;

  // Don't try to transmit if the payload is too big
  if (buf && size <= max_payload_len_udp) {
    // TODO - Do we always send global multicast or link local?
    err = bm_udp_tx_perform(CTX.pcb, buf, size, (const void *)&multicast_global_addr, CTX.port);
  }

  return err;
}

/*!
  @brief Publish data to local device (self)

  @param *buf buffer with pub data

  @return BmOK on success
  @return BmErr on failure
*/
BmErr bm_middleware_local_pub(void *buf, uint32_t size) {
  BmErr err = BmEINVAL;
  NetQueueItem queue_item;

  if (buf) {
    queue_item.buf = buf;
    queue_item.node_id = ip_to_nodeid((void *)bm_ip_get(1));
    queue_item.size = size;

    // add one to reference count since we'll be using it in two places
    err = bm_udp_reference_update(buf);
    if (bm_queue_send(CTX.net_queue, &queue_item, 0) != BmOK) {
      printf("Error sending to Queue\n");
      bm_udp_cleanup(buf);
      err = BmENODEV;
    }
  }

  return err;
}
