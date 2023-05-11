#include <string.h>
#include "ncp.h"
#include "crc.h"

#define MAX_TOPIC_LEN 64

#define NCP_BUFF_LEN 2048
static uint8_t ncp_tx_buff[NCP_BUFF_LEN];

static ncp_callbacks_t _callbacks;

/*!
  Set all the callback functions for NCP

  \param[in] *callbacks pointer to callback structure. This file keeps it's own copy
  \return none
*/
void ncp_set_callbacks(ncp_callbacks_t *callbacks) {
  memcpy(&_callbacks, callbacks, sizeof(ncp_callbacks_t));
}

/*!
  Validate the topic and check that the transmit callback function is set,
  otherwise there's no point

  \param[in] *topic topic string
  \param[in] topic_len length of the topic
  \return BM_NCP_OK if topic is valid, nonzero otherwise
*/
static ncp_error_e _ncp_validate_topic_and_cb(const char *topic, uint16_t topic_len) {
   ncp_error_e rval = BM_NCP_OK;
   do {
    if(!topic) {
      rval = BM_NCP_NULL_BUFF;
      break;
    }

    // Topic too long
    if(topic_len > MAX_TOPIC_LEN) {
      rval = BM_NCP_OVERFLOW;
      break;
    }

    // No transmit function :'(
    if(!_callbacks.tx_fn) {
      rval = BM_NCP_MISSING_CALLBACK;
      break;
    }

  } while(0);

  return rval;
}

/*!
  Get packet buffer with initialized header
  Using static buffer for now (NOT THREAD SAFE)
  If we ever want to allocate or use a message pool, this is where we'd do it

  \param type ncp message type
  \param flags optional flags
  \param buff_len size of required buffer
  \return pointer to buffer if allocated successfully, NULL otherwise
*/
static ncp_packet_t *_ncp_get_packet(ncp_message_t type, uint8_t flags, uint16_t buff_len) {

  if(buff_len <= NCP_BUFF_LEN) {
    ncp_packet_t *packet = reinterpret_cast<ncp_packet_t *>(ncp_tx_buff);

    packet->type = type;
    packet->flags = flags;
    packet->crc16 = 0;

    return packet;
  } else {
    return NULL;
  }
}

/*!
  Send raw NCP data

  \param[in] type NCP message type
  \param[in] *payload message payload
  \param[in] len payload length
  \return BM_NCP_OK if topic is valid, nonzero otherwise
*/
ncp_error_e ncp_tx(ncp_message_t type, const uint8_t *payload, size_t len) {
  ncp_error_e rval = BM_NCP_OK;

  do {
    if(!payload) {
      rval = BM_NCP_NULL_BUFF;
      break;
    }

    // Lets make sure that what we are trying to send will fit in the payload
    if((uint32_t)len + sizeof(ncp_packet_t) >= (NCP_BUFF_LEN - 1)) {
      rval = BM_NCP_OVERFLOW;
      break;
    }

    if(!_callbacks.tx_fn) {
      rval = BM_NCP_MISSING_CALLBACK;
      break;
    }

    uint16_t message_len = sizeof(ncp_packet_t) + len;
    ncp_packet_t *packet = _ncp_get_packet(type, 0, message_len);
    if(!packet) {
      rval = BM_NCP_OUT_OF_MEMORY;
      break;
    }
    memcpy(packet->payload, payload, len);

    packet->crc16 = crc16_ccitt(0, reinterpret_cast<uint8_t *>(packet), message_len);

    uint16_t ncp_message_len = sizeof(ncp_packet_t) + len;
    if(!_callbacks.tx_fn(ncp_tx_buff, ncp_message_len)) {
      rval = BM_NCP_TX_ERR;

      break;
    }

  } while(0);

  return rval;
}

/*!
  NCP publish data to topic

  \param node_id node id of publisher
  \param *topic topic to publish on
  \param topic_len length of topic
  \param *data data to publish
  \param data_len length of data
  \return BM_NCP_OK if ok, nonzero otherwise
*/
ncp_error_e ncp_pub(uint64_t node_id, const char *topic, uint16_t topic_len, const uint8_t *data, uint16_t data_len) {
  ncp_error_e rval = BM_NCP_OK;

  do {
    rval = _ncp_validate_topic_and_cb(topic, topic_len);
    if (rval){
      break;
    }

    uint16_t message_len = sizeof(ncp_packet_t) + sizeof(ncp_pub_header_t) + topic_len + data_len;
    ncp_packet_t *packet = _ncp_get_packet(BM_NCP_PUB, 0, message_len);
    if(!packet) {
      rval = BM_NCP_OUT_OF_MEMORY;
      break;
    }

    ncp_pub_header_t *pub_header = reinterpret_cast<ncp_pub_header_t *>(packet->payload);
    pub_header->node_id = node_id;
    pub_header->topic_len = topic_len;
    memcpy(pub_header->topic, topic, topic_len);

    // Copy data after payload (if any)
    if(data && data_len) {
      memcpy(&pub_header->topic[topic_len], data, data_len);
    }

    packet->crc16 = crc16_ccitt(0, reinterpret_cast<uint8_t *>(packet), message_len);

    if(!_callbacks.tx_fn(ncp_tx_buff, message_len)) {
      rval = BM_NCP_TX_ERR;
      break;
    }

    // TODO - do we wait for an ack?

  } while(0);

  return rval;
}

/*!
  NCP subscribe/unsubscribe from topic

  \param *topic topic to subscribe to
  \param topic_len lenth of topic
  \param sub subscribe/unsubscribe
  \return BM_NCP_OK on success, nonzero otherwise
*/
static ncp_error_e _ncp_sub_unsub(const char *topic, uint16_t topic_len, bool sub) {
  ncp_error_e rval = BM_NCP_OK;

  do {
    rval = _ncp_validate_topic_and_cb(topic, topic_len);
    if (rval){
      break;
    }

    uint16_t message_len = sizeof(ncp_packet_t) + sizeof(ncp_pub_header_t) + topic_len;

    ncp_packet_t *packet = NULL;
    if(sub) {
      packet = _ncp_get_packet(BM_NCP_SUB, 0, message_len);
    } else {
      packet = _ncp_get_packet(BM_NCP_UNSUB, 0, message_len);
    }

    if(!packet) {
      rval = BM_NCP_OUT_OF_MEMORY;
      break;
    }

    ncp_sub_unsub_header_t *sub_header = reinterpret_cast<ncp_sub_unsub_header_t *>(packet->payload);
    sub_header->topic_len = topic_len;
    memcpy(sub_header->topic, topic, topic_len);

    packet->crc16 = crc16_ccitt(0, reinterpret_cast<uint8_t *>(packet), message_len);

    if(!_callbacks.tx_fn(ncp_tx_buff, message_len)) {
      rval = BM_NCP_TX_ERR;
      break;
    }

  } while(0);

  return rval;
}

/*!
  NCP subscribe to topic

  \param *topic topic to subscribe to
  \param topic_len lenth of topic
  \return BM_NCP_OK on success, nonzero otherwise
*/
ncp_error_e ncp_sub(const char *topic, uint16_t topic_len) {
  // TODO - do we wait for an ack?
  return _ncp_sub_unsub(topic, topic_len, true);
}

/*!
  NCP unsubscribe from topic

  \param *topic topic to subscribe to
  \param topic_len lenth of topic
  \return BM_NCP_OK on success, nonzero otherwise
*/
ncp_error_e ncp_unsub(const char *topic, uint16_t topic_len) {
  // TODO - do we want for an ack?
 return _ncp_sub_unsub(topic, topic_len, false);
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
        if(!_callbacks.pub_fn) {
          break;
        }

        ncp_pub_header_t *pub_header = reinterpret_cast<ncp_pub_header_t *>(packet->payload);

        // Protect against topic length being incorrect
        // (would result in overflow when subtracting from len to determine data len)
        uint32_t non_data_len = sizeof(ncp_packet_t) + sizeof(ncp_pub_header_t) + pub_header->topic_len;
        if(non_data_len > len) {
          printf("Invalid topic length!");
          break;
        }

        uint32_t data_len = len - non_data_len;
        _callbacks.pub_fn(reinterpret_cast<const char *>(pub_header->topic),
                          pub_header->topic_len,
                          pub_header->node_id,
                          &pub_header->topic[pub_header->topic_len],
                          data_len);

        break;
      }

      case BM_NCP_SUB: {
        if(!_callbacks.sub_fn) {
          break;
        }

        ncp_sub_unsub_header_t *sub_header = reinterpret_cast<ncp_sub_unsub_header_t *>(packet->payload);
        _callbacks.sub_fn(reinterpret_cast<const char *>(sub_header->topic), sub_header->topic_len);

        break;
      }

      case BM_NCP_UNSUB: {
        if(!_callbacks.unsub_fn) {
          break;
        }

        ncp_sub_unsub_header_t *unsub_header = reinterpret_cast<ncp_sub_unsub_header_t *>(packet->payload);
        _callbacks.unsub_fn(reinterpret_cast<const char *>(unsub_header->topic), unsub_header->topic_len);

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
