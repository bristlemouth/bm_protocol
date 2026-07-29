#include "ncp_metrics.h"
#include "FreeRTOS.h"
#include "bcmp.h"
#include "bm_serial.h"
#include "bridgeLog.h"
#include "cbor.h"
#include "metrics_service.h"
#include "ncp_uart.h"
#include "topology.h"
#include <inttypes.h>
#include <stdlib.h>

#define NCP_METRICS_TIMEOUT_S (5)
#define NCP_METRICS_PRETTY_BUF_LEN (NCP_BUFF_LEN / 2)

typedef struct {
  char *buf;
  size_t cap;
  size_t pos;
} MetricsPrettyState;

static CborError metrics_pretty_stream(void *out, const char *fmt, ...) {
  MetricsPrettyState *state = static_cast<MetricsPrettyState *>(out);
  if (state->pos >= state->cap) {
    return CborNoError; // buffer full: drop remainder
  }
  size_t avail = state->cap - state->pos;
  va_list args;
  va_start(args, fmt);
  int n = vsnprintf(state->buf + state->pos, avail, fmt, args);
  va_end(args);
  if (n > 0) {
    state->pos += (static_cast<size_t>(n) < avail) ? static_cast<size_t>(n) : (avail - 1);
  }
  return CborNoError;
}

static bool ncp_metrics_reply_cb(bool ack, uint32_t msg_id, size_t service_strlen,
                                 const char *service, size_t reply_len, uint8_t *reply_data) {
  (void)msg_id;
  uint64_t node_id = strtoull(service, NULL, 16);
  if (!ack) {
    bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_WARNING, USE_HEADER,
                   "Metrics request NACK'd for %.*s\n", (int)service_strlen, service);
    static const char no_reply[] = "no reply (timed out)";
    bm_serial_send_metrics_reply(node_id, no_reply, sizeof(no_reply) - 1);
    return true;
  }

  static char pretty_buf[NCP_METRICS_PRETTY_BUF_LEN];
  MetricsPrettyState state = {pretty_buf, sizeof(pretty_buf), 0};
  CborParser parser;
  CborValue it;
  if (cbor_parser_init(reply_data, reply_len, 0, &parser, &it) == CborNoError) {
    cbor_value_to_pretty_stream(metrics_pretty_stream, &state, &it, CborPrettyDefaultFlags);
    bm_serial_send_metrics_reply(node_id, pretty_buf, (uint16_t)state.pos);
  }
  return true;
}

// node_id == 0: enumerate the network and request each node
static void ncp_metrics_all_cb(NetworkTopology *networkTopology) {
  if (networkTopology == NULL) {
    bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_WARNING, USE_HEADER,
                   "Metrics: topology unavailable, skipping all-node request\n");
    return;
  }
  uint16_t num_nodes = networkTopology->length;
  NeighborTableEntry *cursor = NULL;
  uint16_t counter;
  for (cursor = networkTopology->front, counter = 0;
       (cursor != NULL) && (counter < num_nodes);
       cursor = cursor->nextNode, counter++) {
    if (cursor->neighbor_table_reply) {
      metrics_service_request(cursor->neighbor_table_reply->node_id, ncp_metrics_reply_cb,
                              NCP_METRICS_TIMEOUT_S);
    }
  }
}

bool ncp_metrics_request_cb(uint64_t node_id) {
  if (node_id == 0) {
    bcmp_topology_start(ncp_metrics_all_cb);
  } else {
    metrics_service_request(node_id, ncp_metrics_reply_cb, NCP_METRICS_TIMEOUT_S);
  }
  return true;
}
