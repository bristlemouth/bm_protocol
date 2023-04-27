#include <string.h>
#include "FreeRTOS.h"
#include "task.h"

#include "bcmp.h"
#include "bcmp_neighbors.h"
#include "bcmp_ping.h"
#include "device_info.h"
#include "uptime.h"

static uint64_t _ping_request_time;

err_t bcmp_send_ping_request(uint64_t node_id, const ip_addr_t *addr, uint8_t* payload, uint16_t payload_len) {

  uint16_t echo_len = sizeof(bcmp_echo_request_t) + payload_len;

  uint8_t *echo_req_buff = (uint8_t *)pvPortMalloc(echo_len);
  configASSERT(echo_req_buff);

  memset(echo_req_buff, 0, echo_len);

  bcmp_echo_request_t *echo_req = (bcmp_echo_request_t *) echo_req_buff;

  echo_req->target_node_id = node_id;
  echo_req->id = 0; // TODO - make this a randomly generated number
  echo_req->seq_num = 0; // TODO - make this an incrementing number
  echo_req->payload_len = payload_len;

  memcpy(&echo_req->payload[0], payload, payload_len);

  err_t rval = bcmp_tx(addr, BCMP_ECHO_REQUEST, (uint8_t*)echo_req, sizeof(*echo_req));

  _ping_request_time = uptimeGetMicroSeconds();

  vPortFree(echo_req_buff);

  return rval;
}

err_t bcmp_send_ping_reply(bcmp_echo_reply_t *echo_reply, const ip_addr_t *addr) {

  return bcmp_tx(addr, BCMP_ECHO_REPLY, (uint8_t*)echo_reply, sizeof(echo_reply));
}

err_t bcmp_process_ping_request(bcmp_echo_request_t *echo_req, const ip_addr_t *src, const ip_addr_t *dst) {
  (void) src;
  // TODO - we will need to reply one the same port, but pass on the request onto the other port?
  // or will it auto pass on based on the multicast and we only have to reply if we have the correct nodeID?
  configASSERT(echo_req);
  if ((echo_req->target_node_id == 0) || (getNodeId() == echo_req->target_node_id)) {
    echo_req->target_node_id = getNodeId();
    return bcmp_send_ping_reply((bcmp_echo_reply_t *)echo_req, dst);
  }

  return ERR_OK;
}

err_t bcmp_process_ping_reply(bcmp_echo_reply_t *echo_reply){
  configASSERT(echo_reply);

  // todo capture a time stamp for when the message was recieved
  uint64_t diff = uptimeGetMicroSeconds() - _ping_request_time;
  printf("🏓 from %016" PRIu64 " that took %" PRIu64 " us\n", echo_reply->node_id, diff);

  return ERR_OK;
}
