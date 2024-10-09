#include <string.h>
#include <cinttypes>
#include "bcmp.h"
#include "bcmp_ping.h"

extern "C" {
#include "bm_os.h"
#include "packet.h"
}

static uint64_t PING_REQUEST_TIMEOUT;
static uint32_t BCMP_SEQ;
static uint8_t *EXPECTED_PAYLOAD = NULL;
static uint16_t EXPECTED_PAYLOAD_LEN = 0;

/*!
  @Brief Send ping to node(s)

  @param node - node id to send ping to (0 for all nodes)
  @param *addr - ip address to send to send ping request to
  @param *payload - payload to send with ping
  @param payload_len - length of payload to send

  @return BmOK if successful
  @return BmErr if failed
*/
BmErr bcmp_send_ping_request(uint64_t node, const void *addr, const uint8_t *payload,
                             uint16_t payload_len) {

  BmErr err = BmENOMEM;
  if (payload == NULL) {
    payload_len = 0;
  }

  uint16_t echo_len = sizeof(BcmpEchoRequest) + payload_len;
  uint8_t *echo_req_buff = (uint8_t *)bm_malloc(echo_len);

  if (echo_req_buff) {
    memset(echo_req_buff, 0, echo_len);

    BcmpEchoRequest *echo_req = (BcmpEchoRequest *)echo_req_buff;

    echo_req->target_node_id = node;
    echo_req->id = (uint16_t)node_id(); // TODO - make this a randomly generated number
    echo_req->seq_num = BCMP_SEQ++;
    echo_req->payload_len = payload_len;

    // clear the expected payload
    if (EXPECTED_PAYLOAD != NULL) {
      bm_free(EXPECTED_PAYLOAD);
      EXPECTED_PAYLOAD = NULL;
      EXPECTED_PAYLOAD_LEN = 0;
    }

    // Lets only copy the paylaod if it isn't NULL just in case
    if (payload != NULL && payload_len > 0) {
      memcpy(&echo_req->payload[0], payload, payload_len);
      EXPECTED_PAYLOAD = (uint8_t *)bm_malloc(payload_len);
      memcpy(EXPECTED_PAYLOAD, payload, payload_len);
      EXPECTED_PAYLOAD_LEN = payload_len;
    }

    printf("PING (%016" PRIx64 "): %" PRIu16 " data bytes\n", echo_req->target_node_id,
           echo_req->payload_len);

    err = bcmp_tx(addr, BcmpEchoRequestMessage, (uint8_t *)echo_req, echo_len, 0);

    PING_REQUEST_TIMEOUT = bm_ticks_to_ms(bm_get_tick_count());

    bm_free(echo_req_buff);
  }

  return err;
}

/*!
  @breif Send ping reply

  @param *echo_reply echo reply message
  @param *addr ip address to send ping reply to

  @return BmOK if successful
  @return BmErr if failed
*/
static BmErr bcmp_send_ping_reply(BcmpEchoReply *echo_reply, void *addr, uint16_t seq_num) {

  return bcmp_tx(addr, BcmpEchoReplyMessage, (uint8_t *)echo_reply,
                 sizeof(*echo_reply) + echo_reply->payload_len, seq_num);
}

/*!
  @brief Handle ping requests

  @param *echo_req echo request message to process
  @param *src source ip of requester
  @param *dst destination ip of request (used for responding to the correct multicast address)

  @return BmOK if successful
  @return BmErr if failed
*/
static BmErr bcmp_process_ping_request(BcmpProcessData data) {
  BcmpEchoRequest *echo_req = (BcmpEchoRequest *)data.payload;
  if ((echo_req->target_node_id == 0) || (node_id() == echo_req->target_node_id)) {
    echo_req->target_node_id = node_id();
    return bcmp_send_ping_reply((BcmpEchoReply *)echo_req, data.dst, echo_req->seq_num);
  }

  return BmOK;
}

/*!
  @brief Handle Ping replies

  @param *echo_reply echo reply message to process
  @param *src source ip of requester
  @param *dst destination ip of request (used for responding to the correct
              multicast address)

  @return BmOK if successful
  @return BmErr if failed
*/
static BmErr bcmp_process_ping_reply(BcmpProcessData data) {
  BmErr err = BmEINVAL;
  BcmpEchoReply *echo_reply = (BcmpEchoReply *)data.payload;

  do {
    if (EXPECTED_PAYLOAD_LEN != echo_reply->payload_len) {
      break;
    }

    // TODO - once we have random numbers working we can then use a static number to check
    if ((uint16_t)node_id() != echo_reply->id) {
      break;
    }

    if (EXPECTED_PAYLOAD != NULL) {
      if (memcmp(EXPECTED_PAYLOAD, echo_reply->payload, echo_reply->payload_len) != 0) {
        break;
      }
    }

    uint64_t diff = bm_ticks_to_ms(bm_get_tick_count()) - PING_REQUEST_TIMEOUT;
    printf("🏓 %" PRIu16 " bytes from %016" PRIx64 " bcmp_seq=%" PRIu32 " time=%" PRIu64
           " ms\n",
           echo_reply->payload_len, echo_reply->node_id, echo_reply->seq_num, diff);
    err = BmOK;

  } while (0);

  return err;
}

/*!
  @brief Initialize Ping Module

  @return BmOK on success
  @return BmErr on failure
 */
BmErr ping_init(void) {
  BcmpPacketCfg ping_request = {
      false,
      false,
      bcmp_process_ping_request,
  };
  BcmpPacketCfg ping_reply = {
      false,
      false,
      bcmp_process_ping_reply,
  };

  BmErr err = BmOK;
  bm_err_check(err, packet_add(&ping_request, BcmpEchoRequestMessage));
  bm_err_check(err, packet_add(&ping_reply, BcmpEchoReplyMessage));
  return err;
}
