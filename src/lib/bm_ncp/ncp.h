#include <stdint.h>
#include <stdio.h>
#include "ncp_messages.h"

typedef struct {
  uint8_t type;
  uint8_t flags;
  uint16_t crc16;
  uint8_t payload[0];
} __attribute__ ((packed)) ncp_packet_t;

typedef bool (*ncp_tx_fn_t)(const uint8_t *buff, size_t len);

void ncp_set_tx_fn(ncp_tx_fn_t tx_fn);
bool ncp_process_packet(ncp_packet_t *packet, size_t len);
int32_t ncp_tx(ncp_message_t type, const uint8_t *buff, size_t len);
