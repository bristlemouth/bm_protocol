#include "network_config_logger.h"
#include "FreeRTOS.h"
#include "bridgeLog.h"
#include "cbor.h"

static char *log_buffer = NULL;
static size_t log_buffer_size = 0;
static constexpr size_t log_buffer_max_size = 2048;

static CborError print_stream_handler(void *out, const char *fmt, ...) {
  configASSERT(log_buffer != NULL);
  (void)out;

  va_list args;
  va_start(args, fmt);
  int n = vsnprintf(nullptr, log_buffer_max_size - log_buffer_size, fmt, args);
  if (n > 0 && log_buffer_size < log_buffer_max_size) {
    vsnprintf(&log_buffer[log_buffer_size], log_buffer_max_size - log_buffer_size, fmt, args);
    log_buffer_size += n;
  }
  va_end(args);

  return n < 0 ? CborErrorIO : CborNoError;
}

void log_cbor_network_configurations(const uint8_t *cbor_buffer, size_t cbor_buffer_size) {
  configASSERT(cbor_buffer != NULL);
  configASSERT(log_buffer == NULL);
  log_buffer = static_cast<char *>(pvPortMalloc(log_buffer_max_size));
  configASSERT(log_buffer);
  log_buffer_size = 0;
  CborParser parser;
  CborValue it;
  CborError err = cbor_parser_init(cbor_buffer, cbor_buffer_size, 0, &parser, &it);
  if (err == CborNoError) {
    cbor_value_to_pretty_stream(print_stream_handler, NULL, &it, CborPrettyDefaultFlags);
    bridgeLogPrint(BRIDGE_CFG, BM_COMMON_LOG_LEVEL_INFO, USE_HEADER,
                   "Bridge network config: %.*s\n", log_buffer_size, log_buffer);
  }
  if (log_buffer) {
    vPortFree(log_buffer);
    log_buffer = NULL;
  }
}
