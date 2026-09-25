#include "metrics_log.h"
#include "bm_config.h"
#include "bm_service_common.h"
#include "mbedtls_base64/base64.h"
#include "spotter.h"

#define METRICS_LOG_FILE "network_metrics.log"
#define METRICS_B64_BUF_SIZE (((MAX_BM_SERVICE_DATA_SIZE + 2) / 3) * 4 + 1)

static uint8_t _b64[METRICS_B64_BUF_SIZE];

void metrics_log_reply_b64(const char *service, size_t service_strlen,
                           const uint8_t *reply_data, size_t reply_len) {
  size_t olen = 0;
  if (mbedtls_base64_encode(_b64, sizeof(_b64), &olen, reply_data, reply_len) == 0) {
    spotter_log(0, METRICS_LOG_FILE, USE_TIMESTAMP, "%.*s %.*s\n",
                (int)service_strlen, service, (int)olen, _b64);
  } else {
    bm_debug("Failed to base64-encode metrics reply\n");
  }
}
