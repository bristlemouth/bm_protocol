#include "mavlink_integration.h"
#include "bm_config.h"
#include "bm_mavlink.h"
#include "bm_os.h"
#include "bm_serial.h"
#include "uptime.h"
#include "util.h"

#define BM_BRIDGE_MAVLINK_SYS_ID 1
#define BM_BRIDGE_MAVLINK_COMP_ID MAV_COMP_ID_USER51

// From flight controller, messages of interest are sent at the same time at a rate of 1Hz
#define BM_USV_TIMEOUT_TIME_MS (500)

static void package_and_send_usv_metrics(uint64_t node_id, mavlink_message_t *msg,
                                         mavlink_status_t *status) {
  (void)status;
  static bool imu_part = false;
  static bool vfr_hud_part = false;
  static bm_serial_usv_metrics_t metrics = {0};
  static uint64_t last_message_ms = 0;

  uint64_t current_ms = uptimeGetMs();
  bool partial_message = imu_part != vfr_hud_part;
  bool timed_out = current_ms - last_message_ms > BM_USV_TIMEOUT_TIME_MS;

  // Clear message if not completed after timeout
  if (partial_message && timed_out) {
    bm_debug("USV message incomplete and timed out after %" PRIu16 "ms, clearing...\n",
             BM_USV_TIMEOUT_TIME_MS);
    imu_part = false;
    vfr_hud_part = false;
    metrics = (bm_serial_usv_metrics_t){0};
  }

  last_message_ms = current_ms;

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
    metrics.xacc = imu.xacc;
    metrics.yacc = imu.yacc;
    metrics.zacc = imu.zacc;
    metrics.xgyro = imu.xgyro;
    metrics.ygyro = imu.ygyro;
    metrics.zgyro = imu.zgyro;
    metrics.xmag = imu.xmag;
    metrics.ymag = imu.ymag;
    metrics.zmag = imu.zmag;
    imu_part = true;
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
    metrics.throttle = vfr_hud.throttle;
    metrics.groundspeed = vfr_hud.groundspeed;
    vfr_hud_part = true;
    break;
  }
  }

  // Message ready to send
  if (vfr_hud_part && imu_part) {
    vfr_hud_part = false;
    imu_part = false;
    bm_serial_send_usv_metrics(metrics);
    metrics = (bm_serial_usv_metrics_t){0};
  }
}

bool bridge_mavlink_init(void) {
  static bool initialized = false;
  static BmMavLinkRxEntry rx_lut[] = {
      {package_and_send_usv_metrics, MAVLINK_MSG_ID_RAW_IMU},
      {package_and_send_usv_metrics, MAVLINK_MSG_ID_VFR_HUD},
  };

  bool ret = true;

  if (!initialized) {
    BmMavLinkInfo info = {
        BM_BRIDGE_MAVLINK_SYS_ID,
        BM_BRIDGE_MAVLINK_COMP_ID,
        MAV_TYPE_GENERIC,
    };

    ret = bm_mavlink_init(info, MAV_STATE_ACTIVE, rx_lut, array_size(rx_lut)) == BmOK;

    if (ret) {
      initialized = true;
    }
  }

  return ret;
}
