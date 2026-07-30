#pragma once
#include <stddef.h>
#include <stdint.h>

// b64-encode a metrics service reply and append it to network_metrics.
// Shared by the periodic sampler and the CLI so the log stays uniform.
void metrics_log_reply_b64(const char *service, size_t service_strlen,
                           const uint8_t *reply_data, size_t reply_len);
