#include "bm_serial.h"
#include "cobs.h"
#include "payload_uart.h"
#include "pubsub.h"
#include "task_priorities.h"
#include <string.h>

static bm_serial_callbacks_t callbacks;
static constexpr size_t rx_buf_size = 2048;
static uint8_t rx_buffer[rx_buf_size];
static size_t rx_idx = 0;
static constexpr size_t packet_buf_size = rx_buf_size;
static uint8_t packet_buffer[rx_buf_size];

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
  bm_serial_set_callbacks(&callbacks);
}

static void reset() {
  rx_idx = 0;
  memset(rx_buffer, 0, rx_buf_size);
}

void loop() {
  if (PLUART::byteAvailable()) {
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
