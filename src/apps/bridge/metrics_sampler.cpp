#include "metrics_sampler.h"

#include "app_config.h"
#include "bm_os.h"
#include "bm_service_common.h"
#include "mbedtls_base64/base64.h"
#include "bridgeLog.h"
#include "configuration.h"
#include "spotter.h"
#include "task_priorities.h"
#include "topology_sampler.h"
#include "metrics_service.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define METRICS_SAMPLER_TASK_STACK_SIZE (1024)
#define METRICS_REQUEST_TIMEOUT_S (5)
#define TOPO_TIMEOUT_MS (10 * 1000)

#define metrics_log_file "network_metrics.log"

#define METRICS_B64_BUF_SIZE (((MAX_BM_SERVICE_DATA_SIZE + 2) / 3) * 4 + 1)

static uint32_t _poll_interval_s;
static uint64_t _node_list[TOPOLOGY_SAMPLER_MAX_NODE_LIST_SIZE];
static uint8_t _b64[METRICS_B64_BUF_SIZE];

static bool metrics_reply_cb(bool ack, uint32_t msg_id, size_t service_strlen,
                             const char *service, size_t reply_len, uint8_t *reply_data) {
  (void)msg_id;
  if (!ack) {
    bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_WARNING, USE_HEADER,
                   "Metrics request NACK'd for %.*s\n", (int)service_strlen, service);
    return true;
  }

  size_t olen = 0;
  if (mbedtls_base64_encode(_b64, sizeof(_b64), &olen, reply_data, reply_len) == 0) {
    spotter_log(0, metrics_log_file, USE_TIMESTAMP, "%.*s %.*s\n",
                (int)service_strlen, service, (int)olen, _b64);
  } else {
    bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_WARNING, USE_HEADER,
                   "Failed to base64-encode metrics reply from %.*s\n",
                   (int)service_strlen, service);
  }
  return true;
}

static void metrics_sampler_task(void *parameters) {
  (void)parameters;
  for (;;) {
    bm_delay(_poll_interval_s * 1000);

    size_t node_list_size = sizeof(_node_list);
    uint32_t num_nodes = 0;
    if (!topology_sampler_get_node_list(_node_list, node_list_size, num_nodes,
                                        TOPO_TIMEOUT_MS)) {
      continue; // no topology discovered yet
    }

    // Note: we intentionally do NOT skip our own node id; the bridge polls its
    // own metrics service too.
    for (uint32_t i = 0; i < num_nodes; i++) {
      if (!metrics_service_request(_node_list[i], metrics_reply_cb,
                                   METRICS_REQUEST_TIMEOUT_S)) {
        bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_WARNING, USE_HEADER,
                       "Failed to send metrics request to %016" PRIx64 "\n", _node_list[i]);
      }
    }
  }
}

void metrics_sampler_init(void) {
  _poll_interval_s = 0;
  get_config_uint(BM_CFG_PARTITION_SYSTEM, AppConfig::METRICS_POLL_INTERVAL_S,
                  strlen(AppConfig::METRICS_POLL_INTERVAL_S), &_poll_interval_s);
  if (_poll_interval_s == 0) {
    bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_INFO, USE_HEADER,
                   "Metrics poll interval is 0; network metrics polling disabled\n");
    return;
  }
  BmErr err = bm_task_create(metrics_sampler_task, "METRICS_SAMPLER",
                             METRICS_SAMPLER_TASK_STACK_SIZE, NULL,
                             METRICS_SAMPLER_TASK_PRIORITY, NULL);
  if (err != BmOK) {
    bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_ERROR, USE_HEADER,
                   "Failed to create metrics sampler task\n");
  }
}
