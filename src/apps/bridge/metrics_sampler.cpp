#include "metrics_sampler.h"

#include "FreeRTOS.h"
#include "task.h"

#include "app_config.h"
#include "bm_os.h"
#include "mbedtls_base64/base64.h"
#include "bridgeLog.h"
#include "configuration.h"
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

  // Raw CBOR travels on the wire; base64 is applied only here
  size_t b64_len = 0;
  mbedtls_base64_encode(NULL, 0, &b64_len, reply_data, reply_len);
  if (b64_len == 0) {
    bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_WARNING, USE_HEADER,
                   "Empty metrics reply from %.*s\n", (int)service_strlen, service);
    return true;
  }

  uint8_t *b64 = (uint8_t *)bm_malloc(b64_len);
  if (b64 == NULL) {
    bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_WARNING, USE_HEADER,
                   "Failed to allocate %u B for metrics b64\n", (unsigned)b64_len);
    return true;
  }

  size_t olen = 0;
  if (mbedtls_base64_encode(b64, b64_len, &olen, reply_data, reply_len) == 0) {
    spotter_log(0, metrics_log_file, USE_TIMESTAMP, "%.*s %.*s\n",
                (int)service_strlen, service, (int)olen, b64);
  } else {
    bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_WARNING, USE_HEADER,
                   "Failed to base64-encode metrics reply from %.*s\n",
                   (int)service_strlen, service);
  }
  bm_free(b64);
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
