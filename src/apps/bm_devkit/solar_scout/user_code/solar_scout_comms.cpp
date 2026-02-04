#include "solar_scout_comms.h"
#include "bm_config.h"
#include "bm_os.h"
#include "payload_uart.h"
#include "task_priorities.h"
#include "uptime.h"

#define SSS_BAUD_RATE 230400
#define SSS_MAX_RETRY_COUNT 3
#define SSS_TIMEOUT_MS 100

static struct SSSInst sss;
static BmSemaphore mutex = NULL;
static BmTimer timer = NULL;

static void timer_cb(BmTimer timer) {
  (void)timer;
  sss_msg_handle_timeout(&sss);
}

static void process_rx_bytes(void *handle, uint8_t byte) {
  (void)handle;
  SSSHandleRet ret = sss_msg_handle_rx(&sss, &byte, sizeof(uint8_t));

  if (ret) {
    //TODO: handle return here
  }
}

static int8_t sss_send(uint8_t *payload, uint16_t len) {
  PLUART::write(payload, len);
  return 0;
}

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

static void sss_timer_start(uint32_t ms) {
  bm_timer_change_period(timer, ms, 0);
  bm_timer_start(timer, 0);
}

static void sss_timer_stop(void) { bm_timer_stop(timer, 0); }

static void sss_lock(void) { bm_semaphore_take(mutex, UINT32_MAX); }

static void sss_unlock(void) { bm_semaphore_give(mutex); }

BmErr solar_scout_comms_init(void) {
  BmErr err = BmOK;
  // Initialize the UART interface
  PLUART::init(USER_TASK_PRIORITY);
  PLUART::setBaud(SSS_BAUD_RATE);
  PLUART::setUseByteStreamBuffer(false);
  PLUART::setUseLineBuffer(false);
  PLUART::setProcessByteCb(process_rx_bytes);
  PLUART::enable();

  // Initialize SSS
  SSSCfg sss_cfg = {
      .send_cb = sss_send,
      .handle_cb = sss_handle_cmd,
      .timer_start_cb = sss_timer_start,
      .timer_stop_cb = sss_timer_stop,
      .lock = sss_lock,
      .unlock = sss_unlock,
      .timeout_ms = SSS_TIMEOUT_MS,
      .retry_count = SSS_MAX_RETRY_COUNT,
  };

  err = sss_init(&sss, sss_cfg) == 0 ? BmOK : BmEIO;

  if (err == 0) {
    timer = bm_timer_create("sss_timer", 10, true, NULL, timer_cb);
    mutex = bm_mutex_create();
  }

  return err;
}

BmErr solar_scout_send_cmd(SSSCommand cmd, uint8_t *payload, uint16_t len) {
  return sss_serialize_message_and_send(&sss, cmd, payload, len) != 0 ? BmOK : BmEINVAL;
}
