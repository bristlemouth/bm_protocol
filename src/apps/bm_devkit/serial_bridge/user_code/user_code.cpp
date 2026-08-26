#include "bm_os.h"
#include "bm_serial.h"
#include "bristlefin.h"
#include "cobs.h"
#include "payload_uart.h"
#include "pubsub.h"
#include "sensors.h"
#include "task_priorities.h"
#include <inttypes.h>
#include <string.h>

typedef enum {
  BM_NONE,
  BM_SUB,
} __attribute__((__packed__)) bm_serial_tx_message_t;
typedef struct {
  uint8_t type;
  uint32_t length;
  uint8_t data[0];
} bm_serial_tx_t;
static bm_serial_callbacks_t callbacks;
static constexpr size_t rx_buf_size = 2048;
static uint8_t rx_buffer[rx_buf_size];
static size_t rx_idx = 0;
static constexpr size_t packet_buf_size = rx_buf_size;
static uint8_t packet_buffer[rx_buf_size];

static bool tx_cb(const uint8_t *buff, size_t len, bm_serial_message_t message) {
  (void)message;
  PLUART::write((uint8_t *)buff, len);
  return true;
}

static void sub_message_cb(uint64_t node_id, const char *topic, uint16_t topic_len,
                           const uint8_t *data, uint16_t data_len, uint8_t type,
                           uint8_t version) {
  printf("Publishing To Topic: %s\n", topic);
  bm_serial_pub(node_id, topic, topic_len, data, data_len, type, version);
}

static bool sub_cb(const char *topic, uint16_t topic_len) {
  return bm_sub_wl(topic, topic_len, sub_message_cb);
}

static bool pub_cb(const char *topic, uint16_t topic_len, uint64_t node_id,
                   const uint8_t *payload, size_t len, uint8_t type, uint8_t version) {
  (void)node_id;
  printf("publishing %u bytes to %.*s\n", len, topic_len, topic);
  return bm_pub_wl(topic, topic_len, payload, len, type, version) == BmOK;
}

void setup() {
  PLUART::init(USER_TASK_PRIORITY);
  PLUART::setBaud(115200);
  PLUART::setUseByteStreamBuffer(true);
  PLUART::setUseLineBuffer(false);
  PLUART::enable();

  memset(&callbacks, 0, sizeof(bm_serial_callbacks_t));
  callbacks.pub_fn = pub_cb;
  callbacks.sub_fn = sub_cb;
  callbacks.tx_fn = tx_cb;
  bm_serial_set_callbacks(&callbacks);
  bristlefin.enableVbus();
  // ensure Vbus stable before enable Vout with a 5ms delay.
  vTaskDelay(pdMS_TO_TICKS(5));
  // enable Vout, 12V by default.
  bristlefin.enableVout();
  // enable 5V out.
  bristlefin.enable5V();
  bristlefin.enable3V();
}

static void reset() {
  rx_idx = 0;
  memset(rx_buffer, 0, rx_buf_size);
}

void loop() {
  while (PLUART::byteAvailable()) {
    const uint8_t b = PLUART::readByte();
    rx_buffer[rx_idx++] = b;
    if (rx_idx >= rx_buf_size) {
      printf("buffer full, resetting\n");
      reset();
    } else {
      if (b == 0) {
        cobs_decode_result result =
            cobs_decode(packet_buffer, packet_buf_size, rx_buffer, rx_idx - 1);
        if (result.status == COBS_DECODE_OK) {
          bm_serial_packet_t *packet = reinterpret_cast<bm_serial_packet_t *>(packet_buffer);
          bm_serial_error_e err = bm_serial_process_packet(packet, result.out_len);
          if (err != BM_SERIAL_OK) {
            printf("packet processing error: %d\n", err);
          }
        } else {
          printf("cobs decode error: %d\n", result.status);
        }
        reset();
      }
    }
  }
}
