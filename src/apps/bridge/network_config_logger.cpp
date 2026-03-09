#include "network_config_logger.h"
#include "FreeRTOS.h"
#include "bm_os.h"
#include "bridgeLog.h"
#include "cbor.h"
#include "ncp_uart.h"

#define CHUNK_SIZE (NCP_BUFF_LEN / 2)

typedef struct {
  char buf[CHUNK_SIZE];
  size_t pos;
} ChunkState;

static CborError print_stream_handler(void *out, const char *fmt, ...) {
  ChunkState *state = static_cast<ChunkState *>(out);

  // Write tmp into our chunk buffer, flushing as needed
  va_list args;
  va_start(args, fmt);
  int n = vsnprintf(NULL, 0, fmt, args);
  va_end(args);

  size_t space_left = CHUNK_SIZE - state->pos;

  // If cannot fit into chunked buffer, send now
  if (space_left < static_cast<size_t>(n)) {
    size_t to_write = state->pos;
    bridgeLogPrint(BRIDGE_CFG, BM_COMMON_LOG_LEVEL_INFO, NO_HEADER, "%.*s", to_write,
                   state->buf);
    state->pos = 0;
    space_left = CHUNK_SIZE;
  }

  va_start(args, fmt);
  state->pos += vsnprintf(&state->buf[state->pos], space_left, fmt, args);
  va_end(args);

  return CborNoError;
}

void log_cbor_network_configurations(const uint8_t *cbor_buffer, size_t cbor_buffer_size) {
  configASSERT(cbor_buffer != NULL);
  static ChunkState state = {};
  CborParser parser;
  CborValue it;
  CborError err = cbor_parser_init(cbor_buffer, cbor_buffer_size, 0, &parser, &it);
  if (err != CborNoError) {
    return;
  }

  state.pos = 0;
  bridgeLogPrint(BRIDGE_CFG, BM_COMMON_LOG_LEVEL_INFO, USE_HEADER, "Bridge network config: ");
  cbor_value_to_pretty_stream(print_stream_handler, &state, &it, CborPrettyDefaultFlags);
  // Flush rest of buffer here
  if (state.pos) {
    size_t to_write = state.pos;
    bridgeLogPrint(BRIDGE_CFG, BM_COMMON_LOG_LEVEL_INFO, NO_HEADER, "%.*s", to_write,
                   state.buf);
  }
  bridgeLogPrint(BRIDGE_CFG, BM_COMMON_LOG_LEVEL_INFO, NO_HEADER, "\n");
}
