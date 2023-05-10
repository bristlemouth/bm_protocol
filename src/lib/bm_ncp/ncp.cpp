#include <string.h>
#include "ncp.h"
#include "third_party/crc/crc.h"

#define NCP_BUFF_LEN 2048
static uint8_t ncp_tx_buff[NCP_BUFF_LEN];

static ncp_callbacks_t _callbacks;

// Set all the callback functions
void ncp_set_callbacks(ncp_callbacks_t *callbacks) {
  memcpy(&_callbacks, callbacks, sizeof(ncp_callbacks_t));
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

    if(!_callbacks.tx_fn) {
      rval = -3;
      break;
    }

    ncp_packet_t *ncp_message = reinterpret_cast<ncp_packet_t *>(ncp_tx_buff);

    ncp_message->type = type;
    ncp_message->flags = 0; // Unused for now
    ncp_message->crc16 = 0;
    memcpy(ncp_message->payload, buff, len);
    ncp_message->crc16 = crc16_ccitt(ncp_message->crc16, ncp_tx_buff, sizeof(ncp_packet_t) + len);

    uint16_t ncp_message_len = sizeof(ncp_packet_t) + len;
    if(!_callbacks.tx_fn(ncp_tx_buff, ncp_message_len)) {
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

    switch(packet->type){
      case BM_NCP_DEBUG: {
        if(_callbacks.debug_fn) {
          _callbacks.debug_fn(packet->payload, len - sizeof(ncp_packet_t));
        }
        break;
      }

      case BM_NCP_PUB: {
        if(_callbacks.pub_fn) {
          // TODO - decode and use actual topic and data
          _callbacks.pub_fn("topic", 0, NULL, 0);
        }
        break;
      }

      case BM_NCP_SUB: {
        if(_callbacks.sub_fn) {
          // TODO - decode and use actual topic
          _callbacks.sub_fn("topic");
        }
        break;
      }

      case BM_NCP_UNSUB: {
        if(_callbacks.unsub_fn) {
          // TODO - decode and use actual topic
          _callbacks.unsub_fn("topic");
        }
        break;
      }

      case BM_NCP_LOG: {
        if(_callbacks.log_fn) {
          // TODO - decode and use actual topic
          _callbacks.log_fn(0, packet->payload, len - sizeof(ncp_packet_t));
        }
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
