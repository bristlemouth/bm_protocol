//
// IEEE 1588-2008 peer-to-peer delay and two-step Sync/Follow_Up over the
// ADIN2111, with this bridge as the grandmaster. The ADIN2111 timer is set
// and disciplined to GPS PPS by bm_serial_adin_utc_cb in ncp_uart.cpp, which
// calls adinPtpTimeSet() each time the timer is (re)set.
//
// Messages use the IEEE 802.3/Ethernet transport (Annex F). Every port runs
// the peer delay mechanism as both the initiator and the responder, and Sync
// and Follow_Up are sent on ports with a neighbor that answers peer delay
// requests. All ADIN2111 access happens in the L2 thread.
//
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "task_priorities.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "adin_ptp.h"
#include "bm_adin2111.h"

extern "C" {
#include "device.h"
#include "l2.h"
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
static constexpr uint32_t PTP_SYNC_INTERVAL_MS = 1000;
static constexpr int8_t PTP_LOG_SYNC_INTERVAL = 0;
static constexpr uint32_t PTP_PDELAY_INTERVAL_MS = 1000;
// Peer delay requests to a port without a responding neighbor back off to this
static constexpr uint32_t PTP_PDELAY_MAX_INTERVAL_MS = 32000;
static constexpr uint32_t PTP_PDELAY_RESP_TIMEOUT_MS = 500;
// Consecutive missing peer delay responses before a port stops sending Sync
static constexpr uint8_t PTP_ALLOWED_LOST_RESPONSES = 3;
static constexpr uint32_t PTP_EGRESS_TS_TIMEOUT_MS = 100;
static constexpr int64_t PTP_MAX_LINK_DELAY_NS = 100000;
static constexpr int64_t PTP_NS_PER_S = 1000000000LL;

static constexpr uint8_t PTP_EVT_QUEUE_LEN = 8;

// Task notification bits
static constexpr uint32_t PTP_NOTIFY_TIME_SET = (1 << 0);
static constexpr uint32_t PTP_NOTIFY_EVT = (1 << 1);
static constexpr uint32_t PTP_NOTIFY_LINK = (1 << 2);

// Egress timestamp capture slot for each message type
static constexpr adi_mac_EgressCapture_e PTP_CAPT_SYNC = ADI_MAC_EGRESS_CAPTURE_A;
static constexpr adi_mac_EgressCapture_e PTP_CAPT_PDELAY_REQ = ADI_MAC_EGRESS_CAPTURE_B;
static constexpr adi_mac_EgressCapture_e PTP_CAPT_PDELAY_RESP = ADI_MAC_EGRESS_CAPTURE_C;

// Message types, control field values and lengths
static constexpr uint8_t PTP_MSG_SYNC = 0x0;
static constexpr uint8_t PTP_MSG_PDELAY_REQ = 0x2;
static constexpr uint8_t PTP_MSG_PDELAY_RESP = 0x3;
static constexpr uint8_t PTP_MSG_FOLLOW_UP = 0x8;
static constexpr uint8_t PTP_MSG_PDELAY_RESP_FOLLOW_UP = 0xA;
static constexpr uint8_t PTP_CTRL_SYNC = 0;
static constexpr uint8_t PTP_CTRL_FOLLOW_UP = 2;
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
static constexpr size_t PTP_OFF_CORRECTION = 8;
static constexpr size_t PTP_OFF_SRC_PORT_ID = 20;
static constexpr size_t PTP_OFF_SEQ = 30;
// Body field offsets, timestamp then (Pdelay_Resp*) requesting port identity
static constexpr size_t PTP_OFF_TS = PTP_HDR_LEN;
static constexpr size_t PTP_OFF_REQ_PORT_ID = PTP_HDR_LEN + 10;

static constexpr size_t ETH_HDR_LEN = 14;
static constexpr size_t ETH_MIN_FRAME_LEN = 60;
static const uint8_t PTP_MAC_PRIMARY[6] = {0x01, 0x1B, 0x19, 0x00, 0x00, 0x00};
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
  size_t len;
  uint8_t msg[PTP_MAX_MSG_LEN];
} PtpEvt_t;

typedef struct {
  uint8_t port_num;
  adi_mac_EgressCapture_e capture;
  size_t len;
  uint8_t frame[ETH_HDR_LEN + PTP_MAX_MSG_LEN];
} PtpTxFrame_t;

// Which parts of a peer delay exchange have been received
static constexpr uint8_t PTP_PDELAY_HAVE_T1 = (1 << 0);
static constexpr uint8_t PTP_PDELAY_HAVE_RESP = (1 << 1);
static constexpr uint8_t PTP_PDELAY_HAVE_RESP_FUP = (1 << 2);
static constexpr uint8_t PTP_PDELAY_HAVE_ALL =
    PTP_PDELAY_HAVE_T1 | PTP_PDELAY_HAVE_RESP | PTP_PDELAY_HAVE_RESP_FUP;

typedef struct {
  bool link_up;
  // Neighbor is answering peer delay requests, Sync is sent to it
  bool capable;
  // Send Sync even if not capable, for the first Sync after the time is set
  bool force_sync;
  uint8_t lost_responses;
  uint32_t pdelay_interval_ms;
  TickType_t next_pdelay;
  TickType_t next_sync;
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

  // Sync
  bool sync_pending;
  TickType_t sync_sent;
  uint16_t sync_seq;
} PtpPort_t;

static TaskHandle_t _task_handle = NULL;
static QueueHandle_t _evt_queue = NULL;
static BridgePowerController *_power_controller = NULL;
static volatile bool _link_state[PTP_NUM_PORTS];
static PtpPort_t _ports[PTP_NUM_PORTS];
static bool _time_valid = false;
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
  @brief Build a PTP message and queue it to be sent from the L2 thread

  @param port_num port to send on
  @param type message type
  @param seq sequence ID
  @param correction correctionField, ns * 2^16
  @param flags0 first octet of flagField
  @param log_interval logMessageInterval
  @param body message body after the header
  @param body_len length of body
  @param capture egress timestamp capture slot

  @return true if queued
 */
static bool send_msg(uint8_t port_num, uint8_t type, uint16_t seq, int64_t correction,
                     uint8_t flags0, int8_t log_interval, const uint8_t *body, size_t body_len,
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
  bool pdelay = (type == PTP_MSG_PDELAY_REQ || type == PTP_MSG_PDELAY_RESP ||
                 type == PTP_MSG_PDELAY_RESP_FOLLOW_UP);
  memcpy(&eth[0], pdelay ? PTP_MAC_PDELAY : PTP_MAC_PRIMARY, 6);
  memcpy(&eth[6], _mac, 6);
  put_be16(&eth[12], ADIN2111_ETHERTYPE_PTP);

  uint8_t *msg = &eth[ETH_HDR_LEN];
  uint8_t control = PTP_CTRL_OTHER;
  if (type == PTP_MSG_SYNC) {
    control = PTP_CTRL_SYNC;
  } else if (type == PTP_MSG_FOLLOW_UP) {
    control = PTP_CTRL_FOLLOW_UP;
  }
  msg[0] = type & 0x0F;
  msg[1] = PTP_VERSION;
  put_be16(&msg[2], static_cast<uint16_t>(msg_len));
  msg[4] = PTP_DOMAIN;
  msg[6] = flags0;
  put_be64(&msg[PTP_OFF_CORRECTION], static_cast<uint64_t>(correction));
  port_id(port_num, &msg[PTP_OFF_SRC_PORT_ID]);
  put_be16(&msg[PTP_OFF_SEQ], seq);
  msg[32] = control;
  msg[33] = static_cast<uint8_t>(log_interval);
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
  uint16_t sync_seq = port->sync_seq;
  memset(port, 0, sizeof(PtpPort_t));
  port->pdelay_seq = pdelay_seq;
  port->sync_seq = sync_seq;
  port->link_up = link_up;
  port->force_sync = link_up;
  port->pdelay_interval_ms = PTP_PDELAY_INTERVAL_MS;
  port->next_pdelay = now;
  port->next_sync = now;
}

static void send_pdelay_req(uint8_t idx, TickType_t now) {
  PtpPort_t *port = &_ports[idx];
  uint8_t body[PTP_PDELAY_LEN - PTP_HDR_LEN] = {};
  port->pdelay_seq++;
  port->pdelay_have = 0;
  port->next_pdelay = now + pdMS_TO_TICKS(port->pdelay_interval_ms);
  if (send_msg(idx + 1, PTP_MSG_PDELAY_REQ, port->pdelay_seq, 0, 0, PTP_LOG_INTERVAL_NONE,
               body, sizeof(body), PTP_CAPT_PDELAY_REQ)) {
    port->pdelay_pending = true;
    port->pdelay_sent = now;
  }
}

static void send_sync(uint8_t idx, TickType_t now) {
  PtpPort_t *port = &_ports[idx];
  // Two-step, so the originTimestamp is left zero and the precise one is in
  // the Follow_Up
  uint8_t body[PTP_SYNC_LEN - PTP_HDR_LEN] = {};
  port->sync_seq++;
  if (send_msg(idx + 1, PTP_MSG_SYNC, port->sync_seq, 0, PTP_FLAG0_TWO_STEP,
               PTP_LOG_SYNC_INTERVAL, body, sizeof(body), PTP_CAPT_SYNC)) {
    port->sync_pending = true;
    port->sync_sent = now;
    port->force_sync = false;
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

static void handle_egress_ts(const PtpEvt_t *evt) {
  uint8_t idx = evt->port_num - 1;
  PtpPort_t *port = &_ports[idx];
  adi_mac_TsTimespec_t ts = ts_add_ns(&evt->ts, ADIN_PTP_TX_LATENCY_NS);
  uint8_t body[PTP_PDELAY_LEN - PTP_HDR_LEN] = {};

  if (evt->capture == PTP_CAPT_SYNC && port->sync_pending) {
    port->sync_pending = false;
    put_ts(body, &ts);
    send_msg(evt->port_num, PTP_MSG_FOLLOW_UP, port->sync_seq, 0, 0, PTP_LOG_SYNC_INTERVAL,
             body, PTP_SYNC_LEN - PTP_HDR_LEN, ADI_MAC_EGRESS_CAPTURE_NONE);
  } else if (evt->capture == PTP_CAPT_PDELAY_REQ && port->pdelay_pending) {
    port->t1 = ts;
    port->pdelay_have |= PTP_PDELAY_HAVE_T1;
    pdelay_try_complete(idx);
  } else if (evt->capture == PTP_CAPT_PDELAY_RESP && port->resp_pending) {
    port->resp_pending = false;
    put_ts(body, &ts);
    memcpy(&body[PTP_OFF_REQ_PORT_ID - PTP_HDR_LEN], port->requester_id, PTP_PORT_ID_LEN);
    send_msg(evt->port_num, PTP_MSG_PDELAY_RESP_FOLLOW_UP, port->resp_seq, port->req_corr, 0,
             PTP_LOG_INTERVAL_NONE, body, sizeof(body), ADI_MAC_EGRESS_CAPTURE_NONE);
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
    if (send_msg(evt->port_num, PTP_MSG_PDELAY_RESP, seq, 0, PTP_FLAG0_TWO_STEP,
                 PTP_LOG_INTERVAL_NONE, body, sizeof(body), PTP_CAPT_PDELAY_RESP)) {
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
  default:
    // This bridge is the grandmaster, other messages are ignored
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
      consider(port->sync_sent + pdMS_TO_TICKS(PTP_EGRESS_TS_TIMEOUT_MS));
    }
    if (_time_valid && port->link_up) {
      if (!port->pdelay_pending) {
        consider(port->next_pdelay);
      }
      consider(port->next_sync);
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
        tick_reached(now, port->sync_sent + pdMS_TO_TICKS(PTP_EGRESS_TS_TIMEOUT_MS))) {
      port->sync_pending = false;
    }

    if (!_time_valid || !port->link_up) {
      continue;
    }

    if (!port->pdelay_pending && tick_reached(now, port->next_pdelay)) {
      send_pdelay_req(idx, now);
    }
    if (tick_reached(now, port->next_sync)) {
      if ((port->capable || port->force_sync) && !port->sync_pending) {
        send_sync(idx, now);
      }
      port->next_sync += pdMS_TO_TICKS(PTP_SYNC_INTERVAL_MS);
      if (tick_reached(now, port->next_sync)) {
        port->next_sync = now + pdMS_TO_TICKS(PTP_SYNC_INTERVAL_MS);
      }
    }
  }
}

static void ptp_task(void *parameters) {
  (void)parameters;

  for (;;) {
    uint32_t notify_bits = 0;
    xTaskNotifyWait(0, UINT32_MAX, &notify_bits, ticks_to_wait(xTaskGetTickCount()));
    TickType_t now = xTaskGetTickCount();

    // The ADIN2111 is reset when the bus powers back up, wait for the timer
    // to be set again before sending anything
    if (_power_controller && !_power_controller->isBridgePowerOn()) {
      if (_time_valid) {
        printf("PTP stopped, bus power is off\n");
      }
      _time_valid = false;
      for (uint8_t idx = 0; idx < PTP_NUM_PORTS; idx++) {
        port_restart(idx, false, now);
      }
      xQueueReset(_evt_queue);
      continue;
    }

    if (notify_bits & PTP_NOTIFY_LINK) {
      for (uint8_t idx = 0; idx < PTP_NUM_PORTS; idx++) {
        bool link_up = _link_state[idx];
        if (link_up != _ports[idx].link_up) {
          port_restart(idx, link_up, now);
        }
      }
    }

    if (notify_bits & PTP_NOTIFY_TIME_SET) {
      // Start peer delay and Sync immediately on every port with a link
      printf("PTP starting, ADIN2111 timer set\n");
      _time_valid = true;
      for (uint8_t idx = 0; idx < PTP_NUM_PORTS; idx++) {
        port_restart(idx, _link_state[idx], now);
      }
    }

    PtpEvt_t evt;
    while (xQueueReceive(_evt_queue, &evt, 0) == pdPASS) {
      if (evt.port_num < 1 || evt.port_num > PTP_NUM_PORTS) {
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

/*!
  @brief Signal the PTP task that the ADIN2111 timer was set

  @details Called by bm_serial_adin_utc_cb each time it sets the ADIN2111 timer
           from the GPS time. A direct to task notification is used since this
           is a data-less event for a single task, it never blocks the caller
           and repeated signals are merged rather than queued.
 */
void adinPtpTimeSet(void) {
  if (_task_handle) {
    xTaskNotify(_task_handle, PTP_NOTIFY_TIME_SET, eSetBits);
  }
}

void adinPtpInit(BridgePowerController *power_controller) {
  _power_controller = power_controller;

  put_be64(_clock_id, node_id());
  BmErr err = mac_address(_mac, sizeof(_mac));
  configASSERT(err == BmOK);

  _evt_queue = xQueueCreate(PTP_EVT_QUEUE_LEN, sizeof(PtpEvt_t));
  configASSERT(_evt_queue);

  BaseType_t rval =
      xTaskCreate(ptp_task, "ADIN_PTP", 1024, NULL, ADIN_PTP_TASK_PRIORITY, &_task_handle);
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
