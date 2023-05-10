#include <stdint.h>
#include <stdio.h>
#include "ncp_messages.h"

typedef struct {
  uint8_t type;
  uint8_t flags;
  uint16_t crc16;
  uint8_t payload[0];
} __attribute__ ((packed)) ncp_packet_t;

typedef struct {
  // Function used to transmit data over the wire
  bool (*tx_fn)(const uint8_t *buff, size_t len);

  // Function called when published data is received
  bool (*pub_fn)(const char *topic, uint64_t node_id, const uint8_t *payload, size_t len);

  // Function called when a subscribe request is received
  bool (*sub_fn)(const char *topic);

  // Function called when an unsusbscribe request is received
  bool (*unsub_fn)(const char *topic);

  // Function called when a log request is received
  bool (*log_fn)(uint64_t node_id, const uint8_t *data, size_t len);

  // Function called when a log request is received
  bool (*debug_fn)(const uint8_t *data, size_t len);
} ncp_callbacks_t;

void ncp_set_callbacks(ncp_callbacks_t *callbacks);
bool ncp_process_packet(ncp_packet_t *packet, size_t len);
int32_t ncp_tx(ncp_message_t type, const uint8_t *buff, size_t len);
