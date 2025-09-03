#include "user_code.h"
#include "bm_os.h"
#include "bm_serial.h"
#include "cobs.h"
#include "payload_uart.h"
#include "pubsub.h"
#include "task_priorities.h"
#include <string.h>

static constexpr size_t RX_BUF_SIZE = 2048;

struct BmSerialBridgeCtx {
  bm_serial_callbacks_t callbacks;
  struct {
    size_t idx;
    uint8_t buffer[RX_BUF_SIZE];
  } rx;
  struct {
    uint8_t buffer[RX_BUF_SIZE];
  } packet;
};

static struct BmSerialBridgeCtx_t ctx; //Serial Bridge Context

/*!
 @brief Serial Transmit Callback For bm_serial Library

 @details Raw bytes (buff) are passed from the bm_serial library to this
          function, which will output data to the PLUART interface.

 @param buff Buffer to transmit serially
 @param len Length of buffer to transmit
 @param message Unused

 @return true always
 */
static bool tx_cb(const uint8_t *buff, size_t len, bm_serial_message_t message) {
  (void)message;
  PLUART::write((uint8_t *)buff, len);
  return true;
}

/*!
 @brief Subscription Callback When Topic Of Interest Is Published To Network

 @details Passes topic information and data serially to serially connected
          device.

 @param node_id Node ID which published the message
 @param topic Topic message was published on
 @param topic_len topic length
 @param data Data associated with topic, can be NULL
 @param data_len Length of data
 @param type Topic type
 @param version Topic version
 */
static void sub_message_cb(uint64_t node_id, const char *topic, uint16_t topic_len,
                           const uint8_t *data, uint16_t data_len, uint8_t type,
                           uint8_t version) {
  if (bm_serial_pub(node_id, topic, topic_len, data, data_len, type, version) != BM_SERIAL_OK) {
    printf("Failed to publish message to serial device\n");

  } else {
    printf("Publishing To Topic: %s\n", topic);
  }
}

/*!
 @brief Subscription Callback For bm_serial Library

 @details When serial message is received for this type of message, subscribe
          to the topic of interest on the Bristlemouth network and pass it
          the received data through to the serially connected device.

 @param topic Topic to subscribe to
 @param topic_len Length of topic

 @return True on success, false on failure
 */
static bool sub_cb(const char *topic, uint16_t topic_len) {
  return bm_sub_wl(topic, topic_len, sub_message_cb) == BmOK;
}

/*!
 @brief Publish Callback For bm_serial Library

 @details Handles publishing data to bristlemouth network from serially
          connected device.

 @param topic Topic to publish to
 @param topic_len Length of topic
 @param node_id Unused
 @param payload Data to publish can be NULL
 @param len Length of data
 @param type Topic type
 @param version Topic version

 @return 
 */
static bool pub_cb(const char *topic, uint16_t topic_len, uint64_t node_id,
                   const uint8_t *payload, size_t len, uint8_t type, uint8_t version) {
  (void)node_id;
  printf("publishing %u bytes to %.*s\n", len, topic_len, topic);
  return bm_pub_wl(topic, topic_len, payload, len, type, version) == BmOK;
}

/*!
 @brief Reset RX Buffer
 */
static void serial_rx_reset() {
  ctx.rx.idx = 0;
  memset(ctx.rx.buffer, 0, RX_BUF_SIZE);
}

void setup() {
  PLUART::init(USER_TASK_PRIORITY);
  PLUART::setBaud(115200);
  PLUART::setUseByteStreamBuffer(true);
  PLUART::setUseLineBuffer(false);
  PLUART::enable();

  memset(&ctx.callbacks, 0, sizeof(bm_serial_callbacks_t));
  ctx.callbacks.pub_fn = pub_cb;
  ctx.callbacks.sub_fn = sub_cb;
  ctx.callbacks.tx_fn = tx_cb;
  bm_serial_set_callbacks(&ctx.callbacks);
  IOWrite(&BB_VBUS_EN, 0);
  // ensure Vbus stable before enable Vout with a 5ms delay.
  vTaskDelay(pdMS_TO_TICKS(5));
  // enable Vout, 12V by default.
  IOWrite(&BB_PL_BUCK_EN, 0);
}

void loop() {
  while (PLUART::byteAvailable()) {
    const uint8_t b = PLUART::readByte();
    ctx.rx.buffer[ctx.rx.idx++] = b;
    if (ctx.rx.idx >= RX_BUF_SIZE) {
      printf("buffer full, resetting\n");
      serial_rx_reset();
    } else {
      if (b == 0) {
        cobs_decode_result result =
            cobs_decode(ctx.packet.buffer, RX_BUF_SIZE, ctx.rx.buffer, ctx.rx.idx - 1);
        if (result.status == COBS_DECODE_OK) {
          bm_serial_packet_t *packet =
              reinterpret_cast<bm_serial_packet_t *>(ctx.packet.buffer);
          bm_serial_error_e err = bm_serial_process_packet(packet, result.out_len);
          if (err != BM_SERIAL_OK) {
            printf("packet processing error: %d\n", err);
          }
        } else {
          printf("cobs decode error: %d\n", result.status);
        }
        serial_rx_reset();
      }
    }
  }
}
