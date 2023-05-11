#include <stdint.h>
#include <stdio.h>
#include "ncp_messages.h"

typedef struct {
  // Function used to transmit data over the wire
  bool (*tx_fn)(const uint8_t *buff, size_t len);

  // Function called when published data is received
  bool (*pub_fn)(const char *topic, uint16_t topic_len, uint64_t node_id, const uint8_t *payload, size_t len);

  // Function called when a subscribe request is received
  bool (*sub_fn)(const char *topic, uint16_t topic_len);

  // Function called when an unsusbscribe request is received
  bool (*unsub_fn)(const char *topic, uint16_t topic_len);

  // Function called when a log request is received
  bool (*log_fn)(uint64_t node_id, const uint8_t *data, size_t len);

  // Function called when a log request is received
  bool (*debug_fn)(const uint8_t *data, size_t len);
} ncp_callbacks_t;

typedef enum {
  BM_NCP_OK = 0,
  BM_NCP_NULL_BUFF = -1,
  BM_NCP_OVERFLOW = -2,
  BM_NCP_MISSING_CALLBACK = -3,
  BM_NCP_OUT_OF_MEMORY = -4,
  BM_NCP_TX_ERR = -5,

  BM_NCP_MISC_ERR,
} ncp_error_e;

void ncp_set_callbacks(ncp_callbacks_t *callbacks);
bool ncp_process_packet(ncp_packet_t *packet, size_t len);
ncp_error_e ncp_tx(ncp_message_t type, const uint8_t *buff, size_t len);

ncp_error_e ncp_pub(uint64_t node_id, const char *topic, uint16_t topic_len, const uint8_t *data, uint16_t data_len);
ncp_error_e ncp_sub(const char *topic, uint16_t topic_len);
ncp_error_e ncp_unsub(const char *topic, uint16_t topic_len);
