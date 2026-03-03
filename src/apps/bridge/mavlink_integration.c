#include "mavlink_integration.h"
#include "bm_config.h"
#include "bm_mavlink.h"
#include "bm_os.h"
#include "bm_serial.h"
#include "uptime.h"
#include "util.h"

#define BM_BRIDGE_MAVLINK_SYS_ID 1
#define BM_BRIDGE_MAVLINK_COMP_ID MAV_COMP_ID_USER51

// From flight controller, messages of interest are sent at a minimum rate of 1Hz
#define BM_USV_TIMEOUT_TIME_MS (1500)

typedef struct {
  bm_serial_usv_metrics_t metrics;
  BmTimer metrics_timer;
  BmSemaphore mut;
} MavlinkIntegrationCtx;

static MavlinkIntegrationCtx ctx = {0};

static void metrics_send_to_ncp(void) {
  bm_serial_send_usv_metrics(ctx.metrics);
  ctx.metrics = (bm_serial_usv_metrics_t){0};
}

static void metrics_timer_cb(void *timer) {
  (void)timer;
  bm_semaphore_take(ctx.mut, BM_MAX_DELAY_UINT32);
  bm_debug("MAVLink metrics timed out...\n");
  metrics_send_to_ncp();
  bm_semaphore_give(ctx.mut);
}

static void package_and_send_usv_metrics(uint64_t node_id, mavlink_message_t *msg,
                                         mavlink_status_t *status) {
  (void)status;

  bm_semaphore_take(ctx.mut, BM_MAX_DELAY_UINT32);
  switch (msg->msgid) {
  case MAVLINK_MSG_ID_RAW_IMU: {
    mavlink_raw_imu_t imu;
    mavlink_msg_raw_imu_decode(msg, &imu);
    bm_debug("Mavlink RAW IMU:\n"
             "\tnode_id: %" PRIx64 "\n"
             "\tsystem_id: %" PRIu8 "\n"
             "\tcomponent_id: %" PRIu8 "\n"
             "\ttime_usec: %" PRIu64 "\n"
             "\txacc: %" PRIi16 "\n"
             "\tyacc: %" PRIi16 "\n"
             "\tzacc: %" PRIi16 "\n"
             "\txgyro: %" PRIi16 "\n"
             "\tygyro: %" PRIi16 "\n"
             "\tzgyro: %" PRIi16 "\n"
             "\txmag: %" PRIi16 "\n"
             "\tymag: %" PRIi16 "\n"
             "\tzmag: %" PRIi16 "\n"
             "\tid: %" PRIu8 "\n"
             "\ttemperature: %" PRIi16 "\n",
             node_id, msg->sysid, msg->compid, imu.time_usec, imu.xacc, imu.yacc, imu.zacc,
             imu.xgyro, imu.ygyro, imu.zgyro, imu.xmag, imu.ymag, imu.zmag, imu.id,
             imu.temperature);
    ctx.metrics.imu.xacc = imu.xacc;
    ctx.metrics.imu.yacc = imu.yacc;
    ctx.metrics.imu.zacc = imu.zacc;
    ctx.metrics.imu.xgyro = imu.xgyro;
    ctx.metrics.imu.ygyro = imu.ygyro;
    ctx.metrics.imu.zgyro = imu.zgyro;
    ctx.metrics.imu.xmag = imu.xmag;
    ctx.metrics.imu.ymag = imu.ymag;
    ctx.metrics.imu.zmag = imu.zmag;
    ctx.metrics.has_imu = true;
    break;
  }
  case MAVLINK_MSG_ID_VFR_HUD: {
    mavlink_vfr_hud_t vfr_hud;
    mavlink_msg_vfr_hud_decode(msg, &vfr_hud);
    bm_debug("Mavlink VFR HUD:\n"
             "\tnode_id: %" PRIx64 "\n"
             "\tsystem_id: %" PRIu8 "\n"
             "\tcomponent_id: %" PRIu8 "\n"
             "\tairspeed: %.3f\n"
             "\tgroundspeed: %.3f\n"
             "\talt: %.3f\n"
             "\tclimb: %.3f\n"
             "\theading: %" PRIi16 "\n"
             "\tthrottle: %" PRIu16 "\n",
             node_id, msg->sysid, msg->compid, vfr_hud.airspeed, vfr_hud.groundspeed,
             vfr_hud.alt, vfr_hud.climb, vfr_hud.heading, vfr_hud.throttle);
    ctx.metrics.vfr.throttle = vfr_hud.throttle;
    ctx.metrics.vfr.groundspeed = vfr_hud.groundspeed;
    ctx.metrics.has_vfr = true;
    break;
  }
  }

  bool partial_message = ctx.metrics.has_imu != ctx.metrics.has_vfr;
  bool full_message = ctx.metrics.has_vfr && ctx.metrics.has_imu;

  // Message ready to send
  if (full_message) {
    bm_debug("MAVLink metrics sending...\n");
    metrics_send_to_ncp();
    bm_timer_stop(ctx.metrics_timer, 0);
  } else if (partial_message) {
    bm_timer_start(ctx.metrics_timer, 0);
  }
  bm_semaphore_give(ctx.mut);
}

bool bridge_mavlink_init(void) {
  static bool initialized = false;
  static BmMavLinkRxEntry rx_lut[] = {
      {package_and_send_usv_metrics, MAVLINK_MSG_ID_RAW_IMU},
      {package_and_send_usv_metrics, MAVLINK_MSG_ID_VFR_HUD},
  };

  if (initialized) {
    return true;
  }

  BmMavLinkInfo info = {
      BM_BRIDGE_MAVLINK_SYS_ID,
      BM_BRIDGE_MAVLINK_COMP_ID,
      MAV_TYPE_GENERIC,
  };

  BmErr err = bm_mavlink_init(info, MAV_STATE_ACTIVE, rx_lut, array_size(rx_lut));
  if (err != BmOK) {
    return false;
  }

  ctx.metrics_timer =
      bm_timer_create("mavlink_timer", BM_USV_TIMEOUT_TIME_MS, false, NULL, metrics_timer_cb);
  if (!ctx.metrics_timer) {
    return false;
  }

  bool ret = false;
  ctx.mut = bm_mutex_create();
  if (ctx.mut) {
    initialized = true;
    ret = true;
  }

  return ret;
}
