#include <string.h>
#include "ncp.h"
#include "third_party/crc/crc.h"

#define NCP_BUFF_LEN 2048
static uint8_t ncp_tx_buff[NCP_BUFF_LEN];

static ncp_tx_fn_t _tx_fn = NULL;

// Set transmit callback function
void ncp_set_tx_fn(ncp_tx_fn_t tx_fn) {
  _tx_fn = tx_fn;
}

// Send ncp data
int32_t ncp_tx(ncp_message_t type, const uint8_t *buff, size_t len) {
  int32_t rval = 0;

  do {
    if(!buff) {
      rval = -1;
      break;
    }

    // Lets make sure that what we are trying to send will fit in the buff
    if((uint32_t)len + sizeof(ncp_packet_t) >= (NCP_BUFF_LEN - 1)) {
      rval = -2;
      break;
    }

    if(!_tx_fn) {
      rval = -3;
      break;
    }

    ncp_packet_t *ncp_message = reinterpret_cast<ncp_packet_t *>(ncp_tx_buff);



    ncp_message->type = type;
    ncp_message->flags = 0; // Unused for now
    ncp_message->crc16 = 0;
    memcpy(ncp_message->payload, buff, len);

    // and then we need to calculate the crc16 on the buffer only?
    ncp_message->crc16 = crc16_ccitt(ncp_message->crc16, ncp_message->payload, len);

    uint16_t ncp_message_len = sizeof(ncp_packet_t) + len;
    if(!_tx_fn(ncp_tx_buff, ncp_message_len)) {
      rval = -4;

      break;
    }

  } while(0);

  return rval;
}

// Process ncp packet (not COBS anymore!)
bool ncp_process_packet(ncp_packet_t *packet, size_t len) {
  bool rval = false;

  // calc the crc16 and compare
  uint16_t crc16_pre = packet->crc16;
  packet->crc16 = 0;
  do {
    uint16_t crc16_post = crc16_ccitt(0, reinterpret_cast<uint8_t *>(packet), len);

    if (crc16_post != crc16_pre) {
      printf("CRC16 invalid!\n");
      break;
    }

    printf("\n");

    switch(packet->type){
      case BCMP_NCP_LOG: {
        printf("Data: ");
        for (uint32_t k = 0; k < (len - sizeof(ncp_packet_t)); k++) {
          printf("%c", (packet->payload[k]));
        }
        break;
      }
      case BCMP_NCP_DEBUG: {
        printf("\nDecoded message: ");
        for(uint32_t j = 0; j < (len - sizeof(ncp_packet_t)); j++) {
          printf("%c", packet->payload[j]);
        }
        printf("\n");
        break;
      }

      default: {
        printf("Header->type %u\n", packet->type);
        printf("Wrong message type!\n");
        break;
      }
    }

    rval = true;
  } while(0);

  return rval;
}
