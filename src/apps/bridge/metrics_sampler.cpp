#include "metrics_sampler.h"

#include "FreeRTOS.h"
#include "task.h"

#include "app_config.h"
#include "bridgeLog.h"
#include "configuration.h"
#include "metrics_service.h"
#include "spotter.h"
#include "task_priorities.h"
#include "topology_sampler.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define DEFAULT_METRICS_POLL_INTERVAL_MS (10 * 1000)
#define METRICS_SAMPLER_TASK_STACK_SIZE (1024)
#define METRICS_REQUEST_TIMEOUT_S (5)
#define TOPO_TIMEOUT_MS (10 * 1000)
#define METRICS_MAX_REPLY_BYTES (256) // MVP cap; current reply is ~80 bytes

#define metrics_log_file "network_metrics.log"

static uint32_t _poll_interval_ms = DEFAULT_METRICS_POLL_INTERVAL_MS;
static uint64_t _node_list[TOPOLOGY_SAMPLER_MAX_NODE_LIST_SIZE];

static bool metrics_reply_cb(bool ack, uint32_t msg_id, size_t service_strlen,
                             const char *service, size_t reply_len, uint8_t *reply_data) {
  (void)msg_id;
  if (!ack) {
    bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_WARNING, USE_HEADER,
                   "Metrics request NACK'd for %.*s\n", (int)service_strlen, service);
    return true;
  }

  size_t n = reply_len;
  if (n > METRICS_MAX_REPLY_BYTES) {
    bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_WARNING, USE_HEADER,
                   "Metrics reply from %.*s truncated: %u > %u bytes\n", (int)service_strlen,
                   service, (unsigned)reply_len, (unsigned)METRICS_MAX_REPLY_BYTES);
    n = METRICS_MAX_REPLY_BYTES;
  }

  char hex[2 * METRICS_MAX_REPLY_BYTES + 1];
  for (size_t i = 0; i < n; i++) {
    snprintf(&hex[i * 2], 3, "%02x", reply_data[i]);
  }
  hex[n * 2] = '\0';
  spotter_log(0, metrics_log_file, USE_TIMESTAMP, "%.*s %s\n", (int)service_strlen, service,
              hex);
  return true;
}

static void metrics_sampler_task(void *parameters) {
  (void)parameters;
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(_poll_interval_ms));

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
  // Poll interval is configurable; fall back to the default and persist it if absent.
  _poll_interval_ms = DEFAULT_METRICS_POLL_INTERVAL_MS;
  if (!get_config_uint(BM_CFG_PARTITION_SYSTEM, AppConfig::METRICS_POLL_INTERVAL_MS,
                       strlen(AppConfig::METRICS_POLL_INTERVAL_MS), &_poll_interval_ms)) {
    bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_INFO, USE_HEADER,
                   "No metrics poll interval in config, using default %" PRIu32
                   " ms and saving to config\n",
                   _poll_interval_ms);
    set_config_uint(BM_CFG_PARTITION_SYSTEM, AppConfig::METRICS_POLL_INTERVAL_MS,
                    strlen(AppConfig::METRICS_POLL_INTERVAL_MS), _poll_interval_ms);
    save_config(BM_CFG_PARTITION_SYSTEM, false);
  }

  BaseType_t rval =
      xTaskCreate(metrics_sampler_task, "METRICS_SAMPLER", METRICS_SAMPLER_TASK_STACK_SIZE,
                  NULL, METRICS_SAMPLER_TASK_PRIORITY, NULL);
  configASSERT(rval == pdPASS);
}
