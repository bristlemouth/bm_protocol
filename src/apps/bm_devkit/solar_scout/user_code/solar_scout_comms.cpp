#include "solar_scout_comms.h"
#include "bm_config.h"
#include "payload_uart.h"
#include "task_priorities.h"
#include "uptime.h"

#define SSS_BAUD_RATE 230400
#define SSS_MAX_RETRY_COUNT 3
#define SSS_TIMEOUT_MS 100

static struct SSSInst sss;

static void sss_service(uint8_t *data, uint16_t len) {

  SSSHandleRet ret = sss_msg_handle(&sss, data, len);

  if (ret) {
    //TODO: handle return here
  }
}

static void process_rx_bytes(void *data, uint8_t len) { sss_service((uint8_t *)data, len); }

static int8_t sss_send(uint8_t *payload, uint16_t len) {
  PLUART::write(payload, len);
  return 0;
}

static uint32_t sss_time(void) { return (uint32_t)uptimeGetMs(); }

static int8_t sss_handle_cmd(SSSCommand type, const uint8_t *payload, uint16_t len, bool *nack,
                             SSSNackReason *reason) {
  switch (type) {
  case SSS_FILE_BEGIN_TRANSFER:
    bm_debug("Beginning File Transfer\n");
    break;

  case SSS_FILE_SEND_DATA:
    bm_debug("File Transfer Data: ");
    for (uint16_t i = 0; i < len; i++) {
      if (i % 32 == 0) {
        bm_debug("\n");
      }
      bm_debug("0x%X ", payload[i]);
    }
    bm_debug("\n");
    break;

  case SSS_FILE_END_TRANSFER:
    bm_debug("Ending File Transfer\n");
    break;

  default:
    *nack = true;
    *reason = NACK_INVALID;
    break;
  }

  return 0;
}

BmErr solar_scout_comms_init(void) {
  // Initialize the UART interface
  PLUART::init(USER_TASK_PRIORITY);
  PLUART::setBaud(SSS_BAUD_RATE);
  PLUART::setUseByteStreamBuffer(false);
  PLUART::setUseLineBuffer(false);
  PLUART::setProcessByteCb(process_rx_bytes);
  PLUART::enable();

  // Initialize SSS
  struct SSSCfg sss_cfg = {
      .send_cb = sss_send,
      .handle_cb = sss_handle_cmd,
      .time_cb = sss_time,
      .timeout_ms = SSS_TIMEOUT_MS,
      .retry_count = SSS_MAX_RETRY_COUNT,
  };
  return sss_init(&sss, sss_cfg) == 0 ? BmOK : BmEIO;
}

BmErr solar_scout_send_cmd(SSSCommand cmd, uint8_t *payload, uint16_t len) {
  return sss_serialize_message_and_send(&sss, cmd, payload, len) != 0 ? BmOK : BmEINVAL;
}
