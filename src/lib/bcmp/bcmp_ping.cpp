#include "FreeRTOS.h"
#include "task.h"
#include <string.h>

#include "bcmp.h"
#include "bcmp_neighbors.h"
#include "bcmp_ping.h"
#include "device_info.h"
#include "uptime.h"

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
  \ret ERR_OK if successful
*/
err_t bcmp_send_ping_request(uint64_t node_id, const ip_addr_t *addr, const uint8_t *payload,
                             uint16_t payload_len) {

  if (payload == NULL) {
    payload_len = 0;
  }

  uint16_t echo_len = sizeof(bcmp_echo_request_t) + payload_len;

  uint8_t *echo_req_buff = static_cast<uint8_t *>(pvPortMalloc(echo_len));
  configASSERT(echo_req_buff);

  memset(echo_req_buff, 0, echo_len);

  bcmp_echo_request_t *echo_req = reinterpret_cast<bcmp_echo_request_t *>(echo_req_buff);

  echo_req->target_node_id = node_id;
  echo_req->id = (uint16_t)getNodeId(); // TODO - make this a randomly generated number
  echo_req->seq_num = _bcmp_seq++;
  echo_req->payload_len = payload_len;

  // clear the expected payload
  if (_expected_payload != NULL) {
    vPortFree(_expected_payload);
    _expected_payload = NULL;
    _expected_payload_len = 0;
  }

  // Lets only copy the paylaod if it isn't NULL just in case
  if (payload != NULL && payload_len > 0) {
    memcpy(&echo_req->payload[0], payload, payload_len);
    _expected_payload = static_cast<uint8_t *>(pvPortMalloc(payload_len));
    memcpy(_expected_payload, payload, payload_len);
    _expected_payload_len = payload_len;
  }

  printf("PING (%016" PRIx64 "): %" PRIu16 " data bytes\n", echo_req->target_node_id,
         echo_req->payload_len);

  err_t rval =
      bcmp_tx(addr, BcmpEchoRequestMessage, reinterpret_cast<uint8_t *>(echo_req), echo_len, 0);

  _ping_request_time = uptimeGetMicroSeconds();

  vPortFree(echo_req_buff);

  return rval;
}

/*!
  Send ping reply

  \param *echo_reply -  echo reply message
  \param *addr - ip address to send ping reply to
  \ret ERR_OK if successful
*/
err_t bcmp_send_ping_reply(bcmp_echo_reply_t *echo_reply, const ip_addr_t *addr,
                           uint16_t seq_num) {

  return bcmp_tx(addr, BcmpEchoReplyMessage, reinterpret_cast<uint8_t *>(echo_reply),
                 sizeof(*echo_reply) + echo_reply->payload_len, seq_num);
}

/*!
  Handle ping requests

  \param *echo_req - echo request message to process
  \param *src - source ip of requester
  \param *dst - destination ip of request (used for responding to the correct multicast address)
  \ret ERR_OK if successful
*/
err_t bcmp_process_ping_request(bcmp_echo_request_t *echo_req, const ip_addr_t *src,
                                const ip_addr_t *dst, uint16_t seq_num) {
  (void)src;
  configASSERT(echo_req);
  if ((echo_req->target_node_id == 0) || (getNodeId() == echo_req->target_node_id)) {
    echo_req->target_node_id = getNodeId();
    return bcmp_send_ping_reply(reinterpret_cast<bcmp_echo_reply_t *>(echo_req), dst, seq_num);
  }

  return ERR_OK;
}

/*!
  Handle Ping replies

  \param *echo_reply - echo reply message to process
  \param *src - source ip of requester
  \param *dst - destination ip of request (used for responding to the correct multicast address)
  \
  */
err_t bcmp_process_ping_reply(bcmp_echo_reply_t *echo_reply) {
  configASSERT(echo_reply);

  err_t rval = ERR_VAL;

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

    uint64_t diff = uptimeGetMicroSeconds() - _ping_request_time;
    printf("🏓 %" PRIu16 " bytes from %016" PRIx64 " bcmp_seq=%" PRIu32 " time=%" PRIu64
           " ms\n",
           echo_reply->payload_len, echo_reply->node_id, echo_reply->seq_num, diff / 1000);
    rval = ERR_OK;

  } while (0);

  return rval;
}
