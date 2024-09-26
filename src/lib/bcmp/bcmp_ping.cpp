#include "bm_os.h"
#include <string.h>

#include "bcmp.h"
#include "bcmp_ping.h"
#include "device_info.h"
extern "C" {
#include "packet.h"
}

static uint64_t _ping_request_time;
static uint32_t _bcmp_seq;
static uint8_t *_expected_payload = NULL;
static uint16_t _expected_payload_len = 0;

/*!
  Send ping to node(s)

  \param node_id - node id to send ping to (0 for all nodes)
  \param *addr - ip address to send to send ping request to
  \param *payload - payload to send with ping
  \param payload_len - length of payload to send
  \ret BmOK if successful
*/
BmErr bcmp_send_ping_request(uint64_t node_id, const void *addr, const uint8_t *payload,
                             uint16_t payload_len) {

  if (payload == NULL) {
    payload_len = 0;
  }

  uint16_t echo_len = sizeof(BcmpEchoRequest) + payload_len;

  uint8_t *echo_req_buff = (uint8_t *)bm_malloc(echo_len);
  // configASSERT(echo_req_buff);

  memset(echo_req_buff, 0, echo_len);

  BcmpEchoRequest *echo_req = (BcmpEchoRequest *)echo_req_buff;

  echo_req->target_node_id = node_id;
  echo_req->id = (uint16_t)getNodeId(); // TODO - make this a randomly generated number
  echo_req->seq_num = _bcmp_seq++;
  echo_req->payload_len = payload_len;

  // clear the expected payload
  if (_expected_payload != NULL) {
    bm_free(_expected_payload);
    _expected_payload = NULL;
    _expected_payload_len = 0;
  }

  // Lets only copy the paylaod if it isn't NULL just in case
  if (payload != NULL && payload_len > 0) {
    memcpy(&echo_req->payload[0], payload, payload_len);
    _expected_payload = (uint8_t *)bm_malloc(payload_len);
    memcpy(_expected_payload, payload, payload_len);
    _expected_payload_len = payload_len;
  }

  printf("PING (%016" PRIx64 "): %" PRIu16 " data bytes\n", echo_req->target_node_id,
         echo_req->payload_len);

  BmErr rval =
      bcmp_tx(addr, BcmpEchoRequestMessage, (uint8_t *)echo_req, echo_len, 0);

  _ping_request_time = bm_ticks_to_ms(bm_get_tick_count());

  bm_free(echo_req_buff);

  return rval;
}

/*!
  Send ping reply

  \param *echo_reply -  echo reply message
  \param *addr - ip address to send ping reply to
  \ret BmOK if successful
*/
static BmErr bcmp_send_ping_reply(BcmpEchoReply *echo_reply,  void *addr,
                           uint16_t seq_num) {

  return bcmp_tx(addr, BcmpEchoReplyMessage, (uint8_t *)echo_reply,
                 sizeof(*echo_reply) + echo_reply->payload_len, seq_num);
}

/*!
  Handle ping requests

  \param *echo_req - echo request message to process
  \param *src - source ip of requester
  \param *dst - destination ip of request (used for responding to the correct multicast address)
  \ret BmOK if successful
*/
static BmErr bcmp_process_ping_request(BcmpProcessData data) {
  BcmpEchoRequest *echo_req = (BcmpEchoRequest *)data.payload;
  // configASSERT(echo_req);
  if ((echo_req->target_node_id == 0) || (getNodeId() == echo_req->target_node_id)) {
    echo_req->target_node_id = getNodeId();
    return bcmp_send_ping_reply((BcmpEchoReply *)echo_req, data.dst, data.seq_num);
  }

  return BmOK;
}

/*!
  Handle Ping replies

  \param *echo_reply - echo reply message to process
  \param *src - source ip of requester
  \param *dst - destination ip of request (used for responding to the correct multicast address)
  \
  */
static BmErr bcmp_process_ping_reply(BcmpProcessData data) {
  BmErr err = BmEINVAL;
  BcmpEchoReply *echo_reply = (BcmpEchoReply *)data.payload;

  do {
    if (_expected_payload_len != echo_reply->payload_len) {
      break;
    }

    // TODO - once we have random numbers working we can then use a static number to check
    if ((uint16_t)getNodeId() != echo_reply->id) {
      break;
    }

    if (_expected_payload != NULL) {
      if (memcmp(_expected_payload, echo_reply->payload, echo_reply->payload_len) != 0) {
        break;
      }
    }

    uint64_t diff = bm_ticks_to_ms(bm_get_tick_count()) - _ping_request_time;
    printf("🏓 %" PRIu16 " bytes from %016" PRIx64 " bcmp_seq=%" PRIu32 " time=%" PRIu64
           " ms\n",
           echo_reply->payload_len, echo_reply->node_id, echo_reply->seq_num, diff);
    err = BmOK;

  } while (0);

  return err;
}


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

  BmErr err;
  err = packet_add(&ping_request, BcmpEchoRequestMessage);
  if (err == BmOK) {
    err = packet_add(&ping_reply, BcmpEchoReplyMessage);
  }
  return err;
}