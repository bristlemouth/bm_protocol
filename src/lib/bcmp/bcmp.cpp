#include <string.h>

#include "bcmp.h"
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

extern "C" {
#include "bm_ip.h"
#include "bm_os.h"
#include "packet.h"
}

#include "bm_dfu.h"

#define bcmp_evt_queue_len 32

// Send heartbeats every 10 seconds (and check for expired links)
#define bcmp_heartbeat_s 10

#define ipv6_header_length (40)

// 1500 MTU minus ipv6 header
#define max_payload_len (1500 - ipv6_header_length)

typedef struct {
  BmQueue queue;
  BmTimer heartbeat_timer;
} bcmpContext_t;

typedef enum {
  BCMP_EVT_RX,
  BCMP_EVT_HEARTBEAT,
} BcmpQueueType;

typedef struct {
  BcmpQueueType type;
  void *data;
  uint32_t size;
} BcmpQueueItem;

static bcmpContext_t CTX;

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
    bcmp_send_heartbeat(bcmp_heartbeat_s);
    bm_timer_start(CTX.heartbeat_timer, 10);
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
static void heartbeat_timer_handler(BmTimer tmr) {
  (void)tmr;

  BcmpQueueItem item = {BCMP_EVT_HEARTBEAT, NULL, 0};

  bm_queue_send(CTX.queue, &item, 0);
}

/*!
  BCMP task. All BCMP events are handled here.

  \param parameters unused
  \return none
*/
static void bcmp_thread(void *parameters) {
  (void)parameters;

  // Start listening for BCMP packets
  CTX.heartbeat_timer =
      bm_timer_create(heartbeat_timer_handler, "bcmp_heartbeat", bcmp_heartbeat_s * 1000, NULL);

  // TODO - send out heartbeats on link change
  for (;;) {
    BcmpQueueItem item;

    BmErr err = bm_queue_receive(CTX.queue, &item, portMAX_DELAY);

    if (err == BmOK) {
      switch (item.type) {
      case BCMP_EVT_RX: {
        process_received_message(item.data, item.size - sizeof(BcmpHeader));
        break;
      }

      case BCMP_EVT_HEARTBEAT: {
        // Should we check neighbors on a differnt timer?
        // Check neighbor status to see if any dropped
        bcmp_check_neighbors();

        // Send out heartbeats
        bcmp_send_heartbeat(bcmp_heartbeat_s);
        break;
      }

      default: {
        break;
      }
      }

      bm_ip_rx_cleanup(item.data);
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
BmErr bcmp_tx(const void *dst, BcmpMessageType type, uint8_t *data, uint16_t size,
              uint32_t seq_num, BmErr (*reply_cb)(uint8_t *payload)) {
  BmErr err = BmEINVAL;
  void *buf = NULL;

  if (dst && (uint32_t)size + sizeof(BcmpHeartbeat) <= max_payload_len) {
    buf = bm_ip_tx_new(dst, size + sizeof(BcmpHeader));
    if (buf) {
      err = serialize(buf, data, size, type, seq_num, reply_cb);

      if (err == BmOK) {
        err = bm_ip_tx_perform(buf, NULL);
        if (err != BmOK) {
          printf("Error sending BMCP packet %d\n", err);
        }
      } else {
        printf("Could not properly serialize message\n");
      }

      bm_ip_tx_cleanup(buf);
    } else {
      printf("Could not allocate memory for bcmp message\n");
    }
  }

  return err;
}

/*!
  \brief Forward the payload to all ports other than the ingress port.

  \details See section 5.4.4.2 of the Bristlemouth spec for details.

  \param[in] pbuf Packet buffer to forward.
  \param[in] ingress_port Port on which the packet was received.
  \return ERR_OK on success, or an error that occurred.
*/
BmErr bcmp_ll_forward(BcmpHeader *header, void *payload, uint32_t size, uint8_t ingress_port) {
  uint8_t port_specific_dst[sizeof(multicast_ll_addr)];
  BmErr err = BmEINVAL;
  void *forward = NULL;
  memcpy(port_specific_dst, &multicast_ll_addr, sizeof(multicast_ll_addr));

  // TODO: Make more generic. This is specifically for a 2-port device.
  uint8_t egress_port = ingress_port == 1 ? 2 : 1;
  ((uint32_t *)port_specific_dst)[3] = 0x1000000 | (egress_port << 8);

  // L2 will clear the egress port from the destination address,
  // so calculate the checksum on the link-local multicast address.
  forward = bm_ip_tx_new(&multicast_ll_addr, size + sizeof(BcmpHeader));
  if (forward) {
    header->checksum = 0;
    header->checksum = packet_checksum(forward, size + sizeof(BcmpHeader));

    // Copy data to be forwarded
    bm_ip_tx_copy(forward, header, sizeof(BcmpHeader), 0);
    bm_ip_tx_copy(forward, payload, size, sizeof(BcmpHeader));

    err = bm_ip_tx_perform(forward, &port_specific_dst);
    if (err != BmOK) {
      printf("Error forwarding BMCP packet link-locally: %d\n", err);
    }
    bm_ip_tx_cleanup(forward);
  } else {
    err = BmENOMEM;
  }
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
BmErr bcmp_init(NvmPartition *dfu_partition, DeviceCfg device) {

  CTX.queue = bm_queue_create(bcmp_evt_queue_len, sizeof(BcmpQueueItem));

  device_init(device);
  bm_ip_init(CTX.queue);

  heartbeat_init();
  ping_init();
  time_init();
  bcmp_topology_init();
  bcmp_process_info_init();

  bm_dfu_init(bcmp_dfu_tx, dfu_partition);
  bcmp_config_init();
  bcmp_resource_discovery_init();

  return bm_task_create(bcmp_thread, "BCMP", 1024, NULL, BCMP_TASK_PRIORITY, NULL);
}
