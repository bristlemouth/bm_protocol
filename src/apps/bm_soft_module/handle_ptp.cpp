//
// IEEE 1588-2008 peer-to-peer delay and two-step Sync/Follow_Up over the
// ADIN2111, with this module as a slave of the bridge (the grandmaster).
//
// Messages use the IEEE 802.3/Ethernet transport (Annex F). Every port runs
// the peer delay mechanism as both the initiator, to measure the link delay to
// the master, and the responder, so the master sees this port as capable and
// sends Sync to it. Each Sync + Follow_Up from the master, together with the
// link delay, gives the offset of the ADIN2111 1588 timer from the master. A PI
// loop on the ADIN2111 TS_ADDEND register keeps the timer locked to the master,
// and offsets too large to slew are stepped. All ADIN2111 access happens in the
// L2 thread.
//
// TS_SEC_CNT and TS_NS_CNT only load the timer, reading them does not return
// the running time. The current ADIN2111 time is instead the seconds of a
// recent ADIN2111 frame timestamp plus the time since the ADIN2111 second
// boundary from TIM2, which pps_timer locks to the ADIN2111 TS_TIMER pulse.
//
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "task_priorities.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "bm_adin2111.h"
#include "handle_ptp.h"
#include "pps_timer.h"

extern "C" {
#include "device.h"
#include "l2.h"
// ADIN2111 driver handle, set by adin2111_Init()
extern adin2111_DeviceHandle_t pDeviceHandle;
}

// Latency between the timestamp point (SFD detected in the PHY) and the wire.
// With an ADIN2111 on both ends of a link these cancel out of the peer delay
// and Sync calculations, so they are only for correcting a measured asymmetry.
#ifndef ADIN_PTP_TX_LATENCY_NS
#define ADIN_PTP_TX_LATENCY_NS 0
#endif
#ifndef ADIN_PTP_RX_LATENCY_NS
#define ADIN_PTP_RX_LATENCY_NS 0
#endif

static constexpr uint8_t PTP_NUM_PORTS = ADIN2111_PORT_NUM;
static constexpr uint32_t PTP_PDELAY_INTERVAL_MS = 1000;
// Peer delay requests to a port without a responding neighbor back off to this
static constexpr uint32_t PTP_PDELAY_MAX_INTERVAL_MS = 32000;
static constexpr uint32_t PTP_PDELAY_RESP_TIMEOUT_MS = 500;
// Consecutive missing peer delay responses before a port's link delay is invalid
static constexpr uint8_t PTP_ALLOWED_LOST_RESPONSES = 3;
static constexpr uint32_t PTP_EGRESS_TS_TIMEOUT_MS = 100;
static constexpr uint32_t PTP_FOLLOW_UP_TIMEOUT_MS = 500;
// Sync messages are followed from one port until it has had none for this long
static constexpr uint32_t PTP_MASTER_TIMEOUT_MS = 3000;
// Syncs to wait for the PPS timer to track the restarted TS_TIMER pulse after a
// step, before stepping again without it
static constexpr uint8_t PTP_STEP_HOLDOFF_SYNCS = 5;
// handlePtpGetTime() takes the seconds from a Sync this recent
static constexpr uint32_t PTP_TIME_REF_MAX_AGE_MS = 60000;
static constexpr int64_t PTP_MAX_LINK_DELAY_NS = 100000;
static constexpr int64_t PTP_NS_PER_S = 1000000000LL;

// Phase errors larger than this step the ADIN2111 timer instead of slewing it
static constexpr int64_t PTP_STEP_THRESHOLD_NS = 1000000;
// PI loop gains, applied as divisors on the phase error (in ns). Sync is sent
// once a second, so a phase error in ns is also a frequency error in ppb.
// Kp = 1/4, Ki = 1/64 gives an overdamped loop (~10s time constant).
static constexpr int64_t PTP_PI_KP_DIV = 4;
static constexpr int64_t PTP_PI_KI_DIV = 64;
// Maximum per-period correction from the proportional term. Phase errors
// larger than this are slewed out at this rate.
static constexpr int64_t PTP_MAX_SLEW_PPB = 100000;
// Maximum frequency correction from the integral term
static constexpr int64_t PTP_MAX_FREQ_PPB = 100000;
static constexpr int64_t PTP_MAX_PHASE_ERR_NS = PTP_MAX_SLEW_PPB * PTP_PI_KP_DIV;
static constexpr int64_t PTP_MAX_FREQ_ACC = PTP_MAX_FREQ_PPB * PTP_PI_KI_DIV;

static constexpr uint8_t PTP_EVT_QUEUE_LEN = 8;

// Task notification bits
static constexpr uint32_t PTP_NOTIFY_EVT = (1 << 0);
static constexpr uint32_t PTP_NOTIFY_LINK = (1 << 1);
static constexpr uint32_t PTP_NOTIFY_STEP = (1 << 2);

// Egress timestamp capture slot for each message type
static constexpr adi_mac_EgressCapture_e PTP_CAPT_PDELAY_REQ = ADI_MAC_EGRESS_CAPTURE_B;
static constexpr adi_mac_EgressCapture_e PTP_CAPT_PDELAY_RESP = ADI_MAC_EGRESS_CAPTURE_C;

// Message types, control field values and lengths
static constexpr uint8_t PTP_MSG_SYNC = 0x0;
static constexpr uint8_t PTP_MSG_PDELAY_REQ = 0x2;
static constexpr uint8_t PTP_MSG_PDELAY_RESP = 0x3;
static constexpr uint8_t PTP_MSG_FOLLOW_UP = 0x8;
static constexpr uint8_t PTP_MSG_PDELAY_RESP_FOLLOW_UP = 0xA;
static constexpr uint8_t PTP_CTRL_OTHER = 5;
static constexpr int8_t PTP_LOG_INTERVAL_NONE = 0x7F;
static constexpr uint8_t PTP_FLAG0_TWO_STEP = 0x02;
static constexpr uint8_t PTP_VERSION = 2;
static constexpr uint8_t PTP_DOMAIN = 0;
static constexpr size_t PTP_HDR_LEN = 34;
static constexpr size_t PTP_SYNC_LEN = 44;
static constexpr size_t PTP_PDELAY_LEN = 54;
static constexpr size_t PTP_MAX_MSG_LEN = 64;
static constexpr size_t PTP_PORT_ID_LEN = 10;

// Header field offsets
static constexpr size_t PTP_OFF_FLAGS = 6;
static constexpr size_t PTP_OFF_CORRECTION = 8;
static constexpr size_t PTP_OFF_SRC_PORT_ID = 20;
static constexpr size_t PTP_OFF_SEQ = 30;
// Body field offsets, timestamp then (Pdelay_Resp*) requesting port identity
static constexpr size_t PTP_OFF_TS = PTP_HDR_LEN;
static constexpr size_t PTP_OFF_REQ_PORT_ID = PTP_HDR_LEN + 10;

static constexpr size_t ETH_HDR_LEN = 14;
static constexpr size_t ETH_MIN_FRAME_LEN = 60;
static const uint8_t PTP_MAC_PDELAY[6] = {0x01, 0x80, 0xC2, 0x00, 0x00, 0x0E};

typedef enum {
  PtpEvtRx,
  PtpEvtEgressTs,
} PtpEvtType_e;

typedef struct {
  PtpEvtType_e type;
  uint8_t port_num;
  bool ts_valid;
  adi_mac_TsTimespec_t ts;
  adi_mac_EgressCapture_e capture;
  // ADIN2111 timer steps before this event, its timestamp is stale if a step followed
  uint32_t epoch;
  TickType_t rx_tick;
  size_t len;
  uint8_t msg[PTP_MAX_MSG_LEN];
} PtpEvt_t;

typedef struct {
  uint8_t port_num;
  adi_mac_EgressCapture_e capture;
  size_t len;
  uint8_t frame[ETH_HDR_LEN + PTP_MAX_MSG_LEN];
} PtpTxFrame_t;

typedef struct {
  // ADIN2111 timestamp of a recent frame and the tick it was received at
  adi_mac_TsTimespec_t ref;
  TickType_t ref_tick;
  // Amount to step the timer by
  int64_t offset_ns;
} PtpStepJob_t;

// Which parts of a peer delay exchange have been received
static constexpr uint8_t PTP_PDELAY_HAVE_T1 = (1 << 0);
static constexpr uint8_t PTP_PDELAY_HAVE_RESP = (1 << 1);
static constexpr uint8_t PTP_PDELAY_HAVE_RESP_FUP = (1 << 2);
static constexpr uint8_t PTP_PDELAY_HAVE_ALL =
    PTP_PDELAY_HAVE_T1 | PTP_PDELAY_HAVE_RESP | PTP_PDELAY_HAVE_RESP_FUP;

typedef struct {
  bool link_up;
  // Neighbor is answering peer delay requests, link_delay_ns is valid
  bool capable;
  uint8_t lost_responses;
  uint32_t pdelay_interval_ms;
  TickType_t next_pdelay;
  int64_t link_delay_ns;

  // Peer delay initiator
  bool pdelay_pending;
  TickType_t pdelay_sent;
  uint16_t pdelay_seq;
  uint8_t pdelay_have;
  adi_mac_TsTimespec_t t1, t2, t3, t4;
  int64_t pdelay_corr_ns;
  uint8_t responder_id[PTP_PORT_ID_LEN];

  // Peer delay responder
  bool resp_pending;
  TickType_t resp_sent;
  uint16_t resp_seq;
  uint8_t requester_id[PTP_PORT_ID_LEN];
  int64_t req_corr;

  // Sync received, waiting for its Follow_Up
  bool sync_pending;
  TickType_t sync_rx_tick;
  uint16_t sync_seq;
  adi_mac_TsTimespec_t sync_t2;
  int64_t sync_corr_ns;
  uint8_t master_id[PTP_PORT_ID_LEN];
} PtpPort_t;

typedef struct {
  // Port the master's Sync messages are followed on, 0 for none
  uint8_t port_num;
  TickType_t last_sync;
  uint16_t last_seq;
  bool have_last;
  int64_t freq_acc;
  // A step is queued to the L2 thread
  bool stepping;
  uint8_t step_holdoff;
  // Last ADIN2111 timer step the task has handled
  uint32_t epoch;
} PtpServo_t;

typedef struct {
  bool valid;
  adi_mac_TsTimespec_t ts;
  TickType_t tick;
} PtpTimeRef_t;

static TaskHandle_t _task_handle = NULL;
static QueueHandle_t _evt_queue = NULL;
static volatile bool _link_state[PTP_NUM_PORTS];
static PtpPort_t _ports[PTP_NUM_PORTS];
static PtpServo_t _servo;
// Incremented in the L2 thread each time the ADIN2111 timer is stepped
static volatile uint32_t _epoch = 0;
// Reference for handlePtpGetTime(), from the last Sync used by the servo
static PtpTimeRef_t _time_ref;
static uint8_t _clock_id[8];
static uint8_t _mac[6];

static void put_be16(uint8_t *buf, uint16_t val) {
  buf[0] = static_cast<uint8_t>(val >> 8);
  buf[1] = static_cast<uint8_t>(val);
}

static uint16_t get_be16(const uint8_t *buf) {
  return static_cast<uint16_t>((buf[0] << 8) | buf[1]);
}

static void put_be64(uint8_t *buf, uint64_t val) {
  for (int i = 0; i < 8; i++) {
    buf[i] = static_cast<uint8_t>(val >> (56 - 8 * i));
  }
}

static uint64_t get_be64(const uint8_t *buf) {
  uint64_t val = 0;
  for (int i = 0; i < 8; i++) {
    val = (val << 8) | buf[i];
  }
  return val;
}

// PTP Timestamp: 48 bit seconds, 32 bit nanoseconds
static void put_ts(uint8_t *buf, const adi_mac_TsTimespec_t *ts) {
  put_be16(&buf[0], 0);
  put_be16(&buf[2], static_cast<uint16_t>(ts->sec >> 16));
  put_be16(&buf[4], static_cast<uint16_t>(ts->sec));
  put_be16(&buf[6], static_cast<uint16_t>(ts->nsec >> 16));
  put_be16(&buf[8], static_cast<uint16_t>(ts->nsec));
}

static void get_ts(const uint8_t *buf, adi_mac_TsTimespec_t *ts) {
  // Upper 16 bits of seconds are ignored, they are zero until 2106
  ts->sec = (static_cast<uint32_t>(get_be16(&buf[2])) << 16) | get_be16(&buf[4]);
  ts->nsec = (static_cast<uint32_t>(get_be16(&buf[6])) << 16) | get_be16(&buf[8]);
}

static adi_mac_TsTimespec_t ts_add_ns(const adi_mac_TsTimespec_t *ts, int64_t ns) {
  int64_t t_ns = static_cast<int64_t>(ts->sec) * PTP_NS_PER_S + ts->nsec + ns;
  if (t_ns < 0) {
    t_ns = 0;
  }
  adi_mac_TsTimespec_t rval = {
      .sec = static_cast<uint32_t>(t_ns / PTP_NS_PER_S),
      .nsec = static_cast<uint32_t>(t_ns % PTP_NS_PER_S),
  };
  return rval;
}

static int64_t ts_sub(adi_mac_TsTimespec_t a, adi_mac_TsTimespec_t b) {
  return adin2111_TsSubtract(&a, &b);
}

static bool tick_reached(TickType_t now, TickType_t deadline) {
  return static_cast<int32_t>(now - deadline) >= 0;
}

static void port_id(uint8_t port_num, uint8_t *id) {
  memcpy(id, _clock_id, sizeof(_clock_id));
  put_be16(&id[8], port_num);
}

/*!
  @brief Current ADIN2111 time

  @details The nanoseconds come from TIM2, which wraps on each ADIN2111 second
           boundary, and the seconds from an ADIN2111 timestamp taken less
           than half a second of tick error ago.

  @param ref ADIN2111 timestamp of a received frame
  @param ref_tick tick the frame was received at
  @param now current ADIN2111 time, estimated from ref and the tick count
             alone if the PPS timer isn't tracking the TS_TIMER pulse

  @return true if the PPS timer was used
 */
static bool adin_time_now(const adi_mac_TsTimespec_t *ref, TickType_t ref_tick,
                          adi_mac_TsTimespec_t *now) {
  int64_t elapsed_ns = static_cast<int64_t>(pdTICKS_TO_MS(xTaskGetTickCount() - ref_tick)) *
                       (PTP_NS_PER_S / 1000);
  adi_mac_TsTimespec_t est = ts_add_ns(ref, elapsed_ns);
  uint64_t since_second_ns;
  if (!ppsTimerGetTimeSinceSecond(&since_second_ns)) {
    *now = est;
    return false;
  }
  // Use the second that puts the PPS timer time closest to the estimate
  now->sec = est.sec;
  now->nsec = static_cast<uint32_t>(since_second_ns);
  int64_t diff = ts_sub(*now, est);
  if (diff > PTP_NS_PER_S / 2) {
    now->sec--;
  } else if (diff < -PTP_NS_PER_S / 2) {
    now->sec++;
  }
  return true;
}

/*!
  @brief Called from the L2 thread to set the ADIN2111 timer frequency offset
         from nominal

  @param arg frequency offset in ppb
 */
static void set_freq_job(void *arg) {
  int64_t ppb = static_cast<int32_t>(reinterpret_cast<intptr_t>(arg));
  int64_t nominal = static_cast<int64_t>(RSTVAL_MAC_TS_ADDEND);
  int64_t addend = nominal + (nominal * ppb) / PTP_NS_PER_S;
  if (adin2111_WriteRegister(pDeviceHandle, ADDR_MAC_TS_ADDEND,
                             static_cast<uint32_t>(addend)) != ADI_ETH_SUCCESS) {
    printf("PTP failed to set ADIN2111 timer frequency\n");
  }
}

/*!
  @brief Called from the L2 thread to step the ADIN2111 timer
 */
static void step_job(void *arg) {
  PtpStepJob_t *job = static_cast<PtpStepJob_t *>(arg);
  adi_mac_TsTimespec_t now;
  bool precise = adin_time_now(&job->ref, job->ref_tick, &now);
  adi_mac_TsTimespec_t target = ts_add_ns(&now, job->offset_ns);
  // TS_TIMER doesn't realign to the new second boundary on its own
  if (adin2111_TsSetTimerAbsolute(pDeviceHandle, target.sec, target.nsec) != ADI_ETH_SUCCESS ||
      adin2111_ts_timer_restart() != BmOK) {
    printf("PTP failed to set ADIN2111 timer\n");
  } else {
    printf("PTP set ADIN2111 timer to %" PRIu32 ".%09" PRIu32 "%s\n", target.sec, target.nsec,
           precise ? "" : ", no PPS timer");
  }
  // The TS_TIMER pulse phase moved with the timer
  ppsTimerReacquire();
  // Timestamps taken before this are stale
  _epoch = _epoch + 1;
  xTaskNotify(_task_handle, PTP_NOTIFY_STEP, eSetBits);
  vPortFree(job);
}

/*!
  @brief Called from the L2 thread to send a PTP frame
 */
static void tx_frame_job(void *arg) {
  PtpTxFrame_t *tx = static_cast<PtpTxFrame_t *>(arg);
  if (adin2111_ptp_send(tx->frame, tx->len, tx->port_num, tx->capture) != BmOK) {
    printf("PTP failed to send on port %u\n", tx->port_num);
  }
  vPortFree(tx);
}

/*!
  @brief Build a peer delay message and queue it to be sent from the L2 thread

  @param port_num port to send on
  @param type message type
  @param seq sequence ID
  @param correction correctionField, ns * 2^16
  @param flags0 first octet of flagField
  @param body message body after the header
  @param body_len length of body
  @param capture egress timestamp capture slot

  @return true if queued
 */
static bool send_msg(uint8_t port_num, uint8_t type, uint16_t seq, int64_t correction,
                     uint8_t flags0, const uint8_t *body, size_t body_len,
                     adi_mac_EgressCapture_e capture) {
  size_t msg_len = PTP_HDR_LEN + body_len;
  PtpTxFrame_t *tx = static_cast<PtpTxFrame_t *>(pvPortMalloc(sizeof(PtpTxFrame_t)));
  if (!tx) {
    return false;
  }
  memset(tx, 0, sizeof(PtpTxFrame_t));
  tx->port_num = port_num;
  tx->capture = capture;
  tx->len = ETH_HDR_LEN + msg_len;
  if (tx->len < ETH_MIN_FRAME_LEN) {
    tx->len = ETH_MIN_FRAME_LEN;
  }

  uint8_t *eth = tx->frame;
  memcpy(&eth[0], PTP_MAC_PDELAY, 6);
  memcpy(&eth[6], _mac, 6);
  put_be16(&eth[12], ADIN2111_ETHERTYPE_PTP);

  uint8_t *msg = &eth[ETH_HDR_LEN];
  msg[0] = type & 0x0F;
  msg[1] = PTP_VERSION;
  put_be16(&msg[2], static_cast<uint16_t>(msg_len));
  msg[4] = PTP_DOMAIN;
  msg[PTP_OFF_FLAGS] = flags0;
  put_be64(&msg[PTP_OFF_CORRECTION], static_cast<uint64_t>(correction));
  port_id(port_num, &msg[PTP_OFF_SRC_PORT_ID]);
  put_be16(&msg[PTP_OFF_SEQ], seq);
  msg[32] = PTP_CTRL_OTHER;
  msg[33] = static_cast<uint8_t>(PTP_LOG_INTERVAL_NONE);
  memcpy(&msg[PTP_HDR_LEN], body, body_len);

  if (bm_l2_run_in_thread(tx_frame_job, tx) != BmOK) {
    vPortFree(tx);
    return false;
  }
  return true;
}

/*!
  @brief Reset a port's protocol state, (re)starting it now if the link is up
 */
static void port_restart(uint8_t idx, bool link_up, TickType_t now) {
  PtpPort_t *port = &_ports[idx];
  uint16_t pdelay_seq = port->pdelay_seq;
  memset(port, 0, sizeof(PtpPort_t));
  port->pdelay_seq = pdelay_seq;
  port->link_up = link_up;
  port->pdelay_interval_ms = PTP_PDELAY_INTERVAL_MS;
  port->next_pdelay = now;
}

static void send_pdelay_req(uint8_t idx, TickType_t now) {
  PtpPort_t *port = &_ports[idx];
  uint8_t body[PTP_PDELAY_LEN - PTP_HDR_LEN] = {};
  port->pdelay_seq++;
  port->pdelay_have = 0;
  port->next_pdelay = now + pdMS_TO_TICKS(port->pdelay_interval_ms);
  if (send_msg(idx + 1, PTP_MSG_PDELAY_REQ, port->pdelay_seq, 0, 0, body, sizeof(body),
               PTP_CAPT_PDELAY_REQ)) {
    port->pdelay_pending = true;
    port->pdelay_sent = now;
  }
}

static void pdelay_lost(uint8_t idx) {
  PtpPort_t *port = &_ports[idx];
  port->pdelay_pending = false;
  if (port->lost_responses < UINT8_MAX) {
    port->lost_responses++;
  }
  if (port->lost_responses >= PTP_ALLOWED_LOST_RESPONSES) {
    if (port->capable) {
      printf("PTP port %d: neighbor stopped answering peer delay requests\n", idx + 1);
    }
    port->capable = false;
    // Back off so a port without a PTP neighbor isn't flooded
    port->pdelay_interval_ms *= 2;
    if (port->pdelay_interval_ms > PTP_PDELAY_MAX_INTERVAL_MS) {
      port->pdelay_interval_ms = PTP_PDELAY_MAX_INTERVAL_MS;
    }
    port->next_pdelay = port->pdelay_sent + pdMS_TO_TICKS(port->pdelay_interval_ms);
  }
}

static void pdelay_try_complete(uint8_t idx) {
  PtpPort_t *port = &_ports[idx];
  if (port->pdelay_have != PTP_PDELAY_HAVE_ALL) {
    return;
  }
  port->pdelay_pending = false;

  int64_t delay =
      (ts_sub(port->t4, port->t1) - ts_sub(port->t3, port->t2) - port->pdelay_corr_ns) / 2;
  if (delay <= -PTP_MAX_LINK_DELAY_NS || delay >= PTP_MAX_LINK_DELAY_NS) {
    printf("PTP port %d: invalid link delay %" PRId64 " ns\n", idx + 1, delay);
    pdelay_lost(idx);
    return;
  }

  port->link_delay_ns = delay;
  port->lost_responses = 0;
  port->pdelay_interval_ms = PTP_PDELAY_INTERVAL_MS;
  if (!port->capable) {
    port->capable = true;
    printf("PTP port %d: neighbor answering peer delay, link delay %" PRId64 " ns\n",
           idx + 1, delay);
  }
}

/*!
  @brief Queue a step of the ADIN2111 timer to the L2 thread

  @param port port the Sync was received on, its timestamp locates the current second
  @param offset_ns amount to step by
 */
static void request_step(const PtpPort_t *port, int64_t offset_ns) {
  PtpStepJob_t *job = static_cast<PtpStepJob_t *>(pvPortMalloc(sizeof(PtpStepJob_t)));
  if (!job) {
    return;
  }
  job->ref = port->sync_t2;
  job->ref_tick = port->sync_rx_tick;
  job->offset_ns = offset_ns;
  taskENTER_CRITICAL();
  _time_ref.valid = false;
  taskEXIT_CRITICAL();
  if (bm_l2_run_in_thread(step_job, job) != BmOK) {
    vPortFree(job);
    return;
  }
  _servo.stepping = true;
}

/*!
  @brief Servo the ADIN2111 timer to the master with a completed Sync + Follow_Up

  @param idx port index
  @param t1 preciseOriginTimestamp, master time the Sync was sent
  @param fup_corr_ns Follow_Up correctionField in ns
  @param now current tick
 */
static void sync_complete(uint8_t idx, const adi_mac_TsTimespec_t *t1, int64_t fup_corr_ns,
                          TickType_t now) {
  PtpPort_t *port = &_ports[idx];
  uint8_t port_num = idx + 1;

  // The link delay to the master is needed, and a step in progress makes t2 stale
  if (!port->capable || _servo.stepping) {
    return;
  }
  // There is no best master clock algorithm, Syncs are followed on the first port
  // they arrive on until it goes quiet
  if (_servo.port_num != port_num) {
    if (_servo.port_num != 0 &&
        !tick_reached(now, _servo.last_sync + pdMS_TO_TICKS(PTP_MASTER_TIMEOUT_MS))) {
      return;
    }
    printf("PTP port %u: following master\n", port_num);
    _servo.port_num = port_num;
    _servo.have_last = false;
  }
  bool consecutive =
      _servo.have_last && port->sync_seq == static_cast<uint16_t>(_servo.last_seq + 1);
  _servo.have_last = true;
  _servo.last_seq = port->sync_seq;
  _servo.last_sync = now;

  // Master time when the Sync was received
  adi_mac_TsTimespec_t master_t2 =
      ts_add_ns(t1, port->link_delay_ns + port->sync_corr_ns + fup_corr_ns);
  // Positive error means the ADIN2111 timer is behind the master (running slow)
  int64_t err = ts_sub(master_t2, port->sync_t2);
  printf("PTP Sync: master %" PRIu32 ".%09" PRIu32 ", ADIN2111 error: %" PRId64 " ns\n",
         master_t2.sec, master_t2.nsec, err);

  uint64_t since_second_ns;
  bool pps_ok = ppsTimerGetTimeSinceSecond(&since_second_ns);

  if (err > PTP_STEP_THRESHOLD_NS || err < -PTP_STEP_THRESHOLD_NS) {
    // After a step, give the PPS timer time to track the restarted TS_TIMER pulse
    // so this step can use the precise current ADIN2111 time
    if (!pps_ok && _servo.step_holdoff > 0) {
      _servo.step_holdoff--;
      return;
    }
    request_step(port, err);
    return;
  }

  bool err_in_range = (err <= PTP_MAX_PHASE_ERR_NS) && (err >= -PTP_MAX_PHASE_ERR_NS);

  // Only integrate small errors measured over a single Sync interval, otherwise
  // the frequency estimate would wind up while slewing or after a Sync gap
  if (consecutive && err_in_range) {
    _servo.freq_acc += err;
    if (_servo.freq_acc > PTP_MAX_FREQ_ACC) {
      _servo.freq_acc = PTP_MAX_FREQ_ACC;
    } else if (_servo.freq_acc < -PTP_MAX_FREQ_ACC) {
      _servo.freq_acc = -PTP_MAX_FREQ_ACC;
    }
  }

  if (err > PTP_MAX_PHASE_ERR_NS) {
    err = PTP_MAX_PHASE_ERR_NS;
  } else if (err < -PTP_MAX_PHASE_ERR_NS) {
    err = -PTP_MAX_PHASE_ERR_NS;
  }

  int64_t ppb = _servo.freq_acc / PTP_PI_KI_DIV + err / PTP_PI_KP_DIV;
  bm_l2_run_in_thread(set_freq_job, reinterpret_cast<void *>(static_cast<intptr_t>(ppb)));

  if (pps_ok) {
    taskENTER_CRITICAL();
    _time_ref.valid = true;
    _time_ref.ts = port->sync_t2;
    _time_ref.tick = port->sync_rx_tick;
    taskEXIT_CRITICAL();
  }
}

static void handle_egress_ts(const PtpEvt_t *evt) {
  uint8_t idx = evt->port_num - 1;
  PtpPort_t *port = &_ports[idx];
  adi_mac_TsTimespec_t ts = ts_add_ns(&evt->ts, ADIN_PTP_TX_LATENCY_NS);
  uint8_t body[PTP_PDELAY_LEN - PTP_HDR_LEN] = {};

  if (evt->capture == PTP_CAPT_PDELAY_REQ && port->pdelay_pending) {
    port->t1 = ts;
    port->pdelay_have |= PTP_PDELAY_HAVE_T1;
    pdelay_try_complete(idx);
  } else if (evt->capture == PTP_CAPT_PDELAY_RESP && port->resp_pending) {
    port->resp_pending = false;
    put_ts(body, &ts);
    memcpy(&body[PTP_OFF_REQ_PORT_ID - PTP_HDR_LEN], port->requester_id, PTP_PORT_ID_LEN);
    send_msg(evt->port_num, PTP_MSG_PDELAY_RESP_FOLLOW_UP, port->resp_seq, port->req_corr, 0,
             body, sizeof(body), ADI_MAC_EGRESS_CAPTURE_NONE);
  }
}

static void handle_rx(const PtpEvt_t *evt, TickType_t now) {
  uint8_t idx = evt->port_num - 1;
  PtpPort_t *port = &_ports[idx];
  const uint8_t *msg = evt->msg;
  uint8_t our_id[PTP_PORT_ID_LEN];
  port_id(evt->port_num, our_id);

  if (evt->len < PTP_HDR_LEN || (msg[1] & 0x0F) != PTP_VERSION || msg[4] != PTP_DOMAIN ||
      get_be16(&msg[2]) > evt->len ||
      memcmp(&msg[PTP_OFF_SRC_PORT_ID], _clock_id, sizeof(_clock_id)) == 0) {
    return;
  }
  uint8_t type = msg[0] & 0x0F;
  uint16_t msg_len = get_be16(&msg[2]);
  uint16_t seq = get_be16(&msg[PTP_OFF_SEQ]);
  int64_t correction = static_cast<int64_t>(get_be64(&msg[PTP_OFF_CORRECTION]));
  adi_mac_TsTimespec_t rx_ts = ts_add_ns(&evt->ts, -ADIN_PTP_RX_LATENCY_NS);

  switch (type) {
  case PTP_MSG_PDELAY_REQ: {
    // A neighbor that sends requests supports peer delay, stop backing off
    if (port->pdelay_interval_ms > PTP_PDELAY_INTERVAL_MS) {
      port->pdelay_interval_ms = PTP_PDELAY_INTERVAL_MS;
      port->lost_responses = 0;
      if (!port->pdelay_pending) {
        port->next_pdelay = now;
      }
    }
    if (msg_len < PTP_PDELAY_LEN || !evt->ts_valid || port->resp_pending) {
      break;
    }
    port->resp_seq = seq;
    port->req_corr = correction;
    memcpy(port->requester_id, &msg[PTP_OFF_SRC_PORT_ID], PTP_PORT_ID_LEN);
    uint8_t body[PTP_PDELAY_LEN - PTP_HDR_LEN];
    put_ts(body, &rx_ts);
    memcpy(&body[PTP_OFF_REQ_PORT_ID - PTP_HDR_LEN], port->requester_id, PTP_PORT_ID_LEN);
    if (send_msg(evt->port_num, PTP_MSG_PDELAY_RESP, seq, 0, PTP_FLAG0_TWO_STEP, body,
                 sizeof(body), PTP_CAPT_PDELAY_RESP)) {
      port->resp_pending = true;
      port->resp_sent = now;
    }
    break;
  }
  case PTP_MSG_PDELAY_RESP: {
    if (msg_len < PTP_PDELAY_LEN || !evt->ts_valid || !port->pdelay_pending ||
        (port->pdelay_have & PTP_PDELAY_HAVE_RESP) || seq != port->pdelay_seq ||
        memcmp(&msg[PTP_OFF_REQ_PORT_ID], our_id, PTP_PORT_ID_LEN) != 0) {
      break;
    }
    get_ts(&msg[PTP_OFF_TS], &port->t2);
    port->t4 = rx_ts;
    port->pdelay_corr_ns = correction >> 16;
    memcpy(port->responder_id, &msg[PTP_OFF_SRC_PORT_ID], PTP_PORT_ID_LEN);
    port->pdelay_have |= PTP_PDELAY_HAVE_RESP;
    pdelay_try_complete(idx);
    break;
  }
  case PTP_MSG_PDELAY_RESP_FOLLOW_UP: {
    if (msg_len < PTP_PDELAY_LEN || !port->pdelay_pending ||
        !(port->pdelay_have & PTP_PDELAY_HAVE_RESP) ||
        (port->pdelay_have & PTP_PDELAY_HAVE_RESP_FUP) || seq != port->pdelay_seq ||
        memcmp(&msg[PTP_OFF_REQ_PORT_ID], our_id, PTP_PORT_ID_LEN) != 0 ||
        memcmp(&msg[PTP_OFF_SRC_PORT_ID], port->responder_id, PTP_PORT_ID_LEN) != 0) {
      break;
    }
    get_ts(&msg[PTP_OFF_TS], &port->t3);
    port->pdelay_corr_ns += correction >> 16;
    port->pdelay_have |= PTP_PDELAY_HAVE_RESP_FUP;
    pdelay_try_complete(idx);
    break;
  }
  case PTP_MSG_SYNC: {
    // Two-step only, the precise origin timestamp comes in the Follow_Up
    if (msg_len < PTP_SYNC_LEN || !evt->ts_valid ||
        !(msg[PTP_OFF_FLAGS] & PTP_FLAG0_TWO_STEP)) {
      break;
    }
    port->sync_pending = true;
    port->sync_rx_tick = evt->rx_tick;
    port->sync_seq = seq;
    port->sync_t2 = rx_ts;
    port->sync_corr_ns = correction >> 16;
    memcpy(port->master_id, &msg[PTP_OFF_SRC_PORT_ID], PTP_PORT_ID_LEN);
    break;
  }
  case PTP_MSG_FOLLOW_UP: {
    if (msg_len < PTP_SYNC_LEN || !port->sync_pending || seq != port->sync_seq ||
        memcmp(&msg[PTP_OFF_SRC_PORT_ID], port->master_id, PTP_PORT_ID_LEN) != 0) {
      break;
    }
    port->sync_pending = false;
    adi_mac_TsTimespec_t t1;
    get_ts(&msg[PTP_OFF_TS], &t1);
    sync_complete(idx, &t1, correction >> 16, now);
    break;
  }
  default:
    break;
  }
}

/*!
  @brief Ticks until the next timeout or scheduled message
 */
static TickType_t ticks_to_wait(TickType_t now) {
  bool found = false;
  TickType_t next = 0;
  auto consider = [&](TickType_t deadline) {
    if (!found || static_cast<int32_t>(deadline - next) < 0) {
      next = deadline;
      found = true;
    }
  };

  for (uint8_t idx = 0; idx < PTP_NUM_PORTS; idx++) {
    PtpPort_t *port = &_ports[idx];
    if (port->pdelay_pending) {
      consider(port->pdelay_sent + pdMS_TO_TICKS(PTP_PDELAY_RESP_TIMEOUT_MS));
    }
    if (port->resp_pending) {
      consider(port->resp_sent + pdMS_TO_TICKS(PTP_EGRESS_TS_TIMEOUT_MS));
    }
    if (port->sync_pending) {
      consider(port->sync_rx_tick + pdMS_TO_TICKS(PTP_FOLLOW_UP_TIMEOUT_MS));
    }
    if (port->link_up && !port->pdelay_pending) {
      consider(port->next_pdelay);
    }
  }

  if (!found) {
    return portMAX_DELAY;
  }
  return tick_reached(now, next) ? 0 : next - now;
}

static void run_ports(TickType_t now) {
  for (uint8_t idx = 0; idx < PTP_NUM_PORTS; idx++) {
    PtpPort_t *port = &_ports[idx];

    if (port->pdelay_pending &&
        tick_reached(now, port->pdelay_sent + pdMS_TO_TICKS(PTP_PDELAY_RESP_TIMEOUT_MS))) {
      pdelay_lost(idx);
    }
    if (port->resp_pending &&
        tick_reached(now, port->resp_sent + pdMS_TO_TICKS(PTP_EGRESS_TS_TIMEOUT_MS))) {
      port->resp_pending = false;
    }
    if (port->sync_pending &&
        tick_reached(now, port->sync_rx_tick + pdMS_TO_TICKS(PTP_FOLLOW_UP_TIMEOUT_MS))) {
      port->sync_pending = false;
    }

    if (port->link_up && !port->pdelay_pending && tick_reached(now, port->next_pdelay)) {
      send_pdelay_req(idx, now);
    }
  }
}

static void ptp_task(void *parameters) {
  (void)parameters;

  for (;;) {
    uint32_t notify_bits = 0;
    xTaskNotifyWait(0, UINT32_MAX, &notify_bits, ticks_to_wait(xTaskGetTickCount()));
    TickType_t now = xTaskGetTickCount();

    if (notify_bits & PTP_NOTIFY_LINK) {
      for (uint8_t idx = 0; idx < PTP_NUM_PORTS; idx++) {
        bool link_up = _link_state[idx];
        if (link_up != _ports[idx].link_up) {
          port_restart(idx, link_up, now);
        }
      }
    }

    // After a step, drop exchanges with timestamps from before it
    uint32_t epoch = _epoch;
    if (epoch != _servo.epoch) {
      _servo.epoch = epoch;
      _servo.stepping = false;
      _servo.have_last = false;
      _servo.step_holdoff = PTP_STEP_HOLDOFF_SYNCS;
      for (uint8_t idx = 0; idx < PTP_NUM_PORTS; idx++) {
        PtpPort_t *port = &_ports[idx];
        port->resp_pending = false;
        port->sync_pending = false;
        if (port->pdelay_pending) {
          port->pdelay_pending = false;
          port->next_pdelay = now;
        }
      }
    }

    PtpEvt_t evt;
    while (xQueueReceive(_evt_queue, &evt, 0) == pdPASS) {
      if (evt.port_num < 1 || evt.port_num > PTP_NUM_PORTS || evt.epoch != epoch) {
        continue;
      }
      if (evt.type == PtpEvtRx) {
        handle_rx(&evt, now);
      } else if (evt.type == PtpEvtEgressTs) {
        handle_egress_ts(&evt);
      }
    }

    run_ports(now);
  }
}

static void post_evt(const PtpEvt_t *evt) {
  if (xQueueSend(_evt_queue, evt, 0) == pdPASS) {
    xTaskNotify(_task_handle, PTP_NOTIFY_EVT, eSetBits);
  }
}

/*!
  @brief Called from the L2 thread for each received PTP frame
 */
static void rx_cb(uint8_t port_num, const uint8_t *data, size_t length,
                  const adi_mac_TsTimespec_t *rx_ts) {
  if (length < ETH_HDR_LEN + PTP_HDR_LEN) {
    return;
  }
  PtpEvt_t evt = {};
  evt.type = PtpEvtRx;
  evt.port_num = port_num;
  evt.ts_valid = (rx_ts != NULL);
  if (rx_ts) {
    evt.ts = *rx_ts;
  }
  evt.epoch = _epoch;
  evt.rx_tick = xTaskGetTickCount();
  evt.len = length - ETH_HDR_LEN;
  if (evt.len > PTP_MAX_MSG_LEN) {
    evt.len = PTP_MAX_MSG_LEN;
  }
  memcpy(evt.msg, &data[ETH_HDR_LEN], evt.len);
  post_evt(&evt);
}

/*!
  @brief Called from the L2 thread to read an egress timestamp, outside of the
         ADIN2111 interrupt handling the ready callback is called from

  @param arg port number in bits 7:4, capture slot in bits 3:0
 */
static void read_egress_ts_job(void *arg) {
  uintptr_t val = reinterpret_cast<uintptr_t>(arg);
  PtpEvt_t evt = {};
  evt.type = PtpEvtEgressTs;
  evt.port_num = static_cast<uint8_t>(val >> 4);
  evt.capture = static_cast<adi_mac_EgressCapture_e>(val & 0xF);
  evt.epoch = _epoch;
  if (adin2111_ptp_get_egress_timestamp(evt.port_num, evt.capture, &evt.ts) == BmOK) {
    evt.ts_valid = true;
    post_evt(&evt);
  } else {
    printf("PTP failed to read egress timestamp\n");
  }
}

static void egress_ts_ready_cb(uint8_t port_num, adi_mac_EgressCapture_e capture) {
  uintptr_t val = (static_cast<uintptr_t>(port_num) << 4) | (capture & 0xF);
  bm_l2_run_in_thread(read_egress_ts_job, reinterpret_cast<void *>(val));
}

static void link_change_cb(uint8_t port_num, bool state) {
  if (port_num >= 1 && port_num <= PTP_NUM_PORTS) {
    _link_state[port_num - 1] = state;
    xTaskNotify(_task_handle, PTP_NOTIFY_LINK, eSetBits);
  }
}

bool handlePtpGetTime(uint64_t *ns) {
  configASSERT(ns);
  taskENTER_CRITICAL();
  PtpTimeRef_t ref = _time_ref;
  taskEXIT_CRITICAL();

  if (!ref.valid ||
      tick_reached(xTaskGetTickCount(), ref.tick + pdMS_TO_TICKS(PTP_TIME_REF_MAX_AGE_MS))) {
    return false;
  }
  adi_mac_TsTimespec_t now;
  if (!adin_time_now(&ref.ts, ref.tick, &now)) {
    return false;
  }
  *ns = static_cast<uint64_t>(now.sec) * PTP_NS_PER_S + now.nsec;
  return true;
}

void handlePtpInit(void) {
  put_be64(_clock_id, node_id());
  BmErr err = mac_address(_mac, sizeof(_mac));
  configASSERT(err == BmOK);

  _evt_queue = xQueueCreate(PTP_EVT_QUEUE_LEN, sizeof(PtpEvt_t));
  configASSERT(_evt_queue);

  BaseType_t rval =
      xTaskCreate(ptp_task, "handle_ptp", 1024, NULL, HANDLE_PTP_TASK_PRIORITY, &_task_handle);
  configASSERT(rval == pdPASS);

  static const Adin2111PtpCallbacks callbacks = {
      .receive = rx_cb,
      .egress_timestamp_ready = egress_ts_ready_cb,
  };
  err = adin2111_ptp_register_callbacks(&callbacks);
  configASSERT(err == BmOK);
  err = bm_l2_register_link_change_callback(link_change_cb);
  configASSERT(err == BmOK);
}
