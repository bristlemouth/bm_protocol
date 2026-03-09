#include "network_config_logger.h"
#include "FreeRTOS.h"
#include "bridgeLog.h"
#include "cbor.h"
#include "ncp_uart.h"

static char *log_buffer = NULL;
static size_t log_buffer_size = 0;
static size_t log_buffer_offset = 0;

static CborError size_handler(void *out, const char *fmt, ...) {
  (void)out;

  va_list args;
  va_start(args, fmt);
  int n = vsnprintf(nullptr, 0, fmt, args);
  log_buffer_size += n;
  va_end(args);

  return n < 0 ? CborErrorIO : CborNoError;
}

static CborError print_stream_handler(void *out, const char *fmt, ...) {
  configASSERT(log_buffer != NULL);
  (void)out;

  va_list args;
  va_start(args, fmt);
  log_buffer_offset +=
      vsnprintf(&log_buffer[log_buffer_offset], log_buffer_size - log_buffer_offset, fmt, args);
  va_end(args);

  return CborNoError;
}

void log_cbor_network_configurations(const uint8_t *cbor_buffer, size_t cbor_buffer_size) {
  configASSERT(cbor_buffer != NULL);
  log_buffer_size = 0;
  log_buffer_offset = 0;
  CborParser parser;
  CborValue it;
  CborError err = cbor_parser_init(cbor_buffer, cbor_buffer_size, 0, &parser, &it);
  if (err != CborNoError) {
    return;
  }

  // Obtain log buffer size
  err = cbor_value_to_pretty_stream(size_handler, NULL, &it, CborPrettyDefaultFlags);
  if (err != CborNoError) {
    return;
  }

  // Must re-initialize parser's iterator
  err = cbor_parser_init(cbor_buffer, cbor_buffer_size, 0, &parser, &it);
  if (err != CborNoError) {
    return;
  }

  // Fail gracefully here, the system can still operate without sending this message
  log_buffer = static_cast<char *>(pvPortMalloc(log_buffer_size));
  if (!log_buffer) {
    return;
  }

  cbor_value_to_pretty_stream(print_stream_handler, NULL, &it, CborPrettyDefaultFlags);
  bridgeLogPrint(BRIDGE_CFG, BM_COMMON_LOG_LEVEL_INFO, USE_HEADER, "Bridge network config: ");

  // Chunk the config map to be sent over NCP UART
  constexpr uint32_t print_max_size = NCP_BUFF_LEN / 2;
  for (uint32_t i = 0; i < log_buffer_size; i += print_max_size) {
    uint32_t log_send_size =
        log_buffer_size > print_max_size ? print_max_size : log_buffer_size;
    bridgeLogPrint(BRIDGE_CFG, BM_COMMON_LOG_LEVEL_INFO, NO_HEADER, "%.*s", log_send_size,
                   &log_buffer[i]);
  }
  bridgeLogPrint(BRIDGE_CFG, BM_COMMON_LOG_LEVEL_INFO, NO_HEADER, "\n");

  vPortFree(log_buffer);
  log_buffer = NULL;
}
