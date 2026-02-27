#include "user_code.h"
#include "bm_config.h"
#include "bm_mavlink.h"
#include "bsp.h"
#include "payload_uart.h"
#include "spotter.h"
#include "task_priorities.h"

#define BM_MAVLINK_BRIDGE_COMPONENT_ID 20
#define BM_MAVLINK_BRIDGE_BAUD 57600

static void process_rx_bytes(uint8_t byte) {
  // Bristlemouth's Middleware uses MAVLINK_COMM_0, other channels MUST be
  // utilized for parsing other MAVLink streams
  static constexpr uint8_t chan = MAVLINK_COMM_1;
  mavlink_status_t status = {};
  mavlink_message_t msg = {};

  bool message_found = mavlink_parse_char(chan, byte, &msg, &status);
  if (message_found) {
    bm_debug("Received MAVLink Message ID: %d\n", msg.msgid);

    switch (msg.msgid) {
    case MAVLINK_MSG_ID_RAW_IMU: {
      mavlink_raw_imu_t imu;
      mavlink_msg_raw_imu_decode(&msg, &imu);
      bm_debug("Mavlink RAW IMU:\n"
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
               msg.sysid, msg.compid, imu.time_usec, imu.xacc, imu.yacc, imu.zacc, imu.xgyro,
               imu.ygyro, imu.zgyro, imu.xmag, imu.ymag, imu.zmag, imu.id, imu.temperature);
      bm_mavlink_transmit(&msg);
      break;
    }
    case MAVLINK_MSG_ID_VFR_HUD:
      mavlink_vfr_hud_t vfr_hud;
      mavlink_msg_vfr_hud_decode(&msg, &vfr_hud);
      bm_debug("Mavlink VFR HUD:\n"
               "\tsystem_id: %" PRIu8 "\n"
               "\tcomponent_id: %" PRIu8 "\n"
               "\tairspeed: %.3f\n"
               "\tgroundspeed: %.3f\n"
               "\talt: %.3f\n"
               "\tclimb: %.3f\n"
               "\theading: %" PRIi16 "\n"
               "\tthrottle: %" PRIu16 "\n",
               msg.sysid, msg.compid, vfr_hud.airspeed, vfr_hud.groundspeed, vfr_hud.alt,
               vfr_hud.climb, vfr_hud.heading, vfr_hud.throttle);
      bm_mavlink_transmit(&msg);
    }
  }
}

static void heartbeat_cb(uint64_t node_id, mavlink_message_t *msg, mavlink_status_t *status) {
  (void)status;
  mavlink_heartbeat_t heartbeat;
  mavlink_msg_heartbeat_decode(msg, &heartbeat);

  bm_debug("Mavlink Heartbeat Message:\n"
           "\tBristlemouth node_id: %" PRIx64 "\n"
           "\tsystem_id: %" PRIu8 "\n"
           "\tcomponent_id: %" PRIu8 "\n"
           "\ttype: %" PRIu8 "\n"
           "\tautopilot: %" PRIu8 "\n"
           "\tbase_mode: %" PRIu8 "\n"
           "\tsystem_status: %" PRIu8 "\n"
           "\tmavlink_version: %" PRIu8 "\n",
           node_id, msg->sysid, msg->compid, heartbeat.type, heartbeat.autopilot,
           heartbeat.base_mode, heartbeat.system_status, heartbeat.mavlink_version);
}

// MAVLink messages of interest and associated callbacks located here
static BmMavLinkRxEntry rx_lut[] = {
    {heartbeat_cb, MAVLINK_MSG_ID_HEARTBEAT},
};

void setup(void) {
  // Setup Bristlemouth MAVLink integration
  bm_mavlink_init({1, BM_MAVLINK_BRIDGE_COMPONENT_ID, MAV_TYPE_GENERIC}, MAV_STATE_ACTIVE,
                  rx_lut, array_size(rx_lut));

  // Setup payload uart to receive incoming MAVLink messages
  PLUART::init(USER_TASK_PRIORITY);
  PLUART::setBaud(BM_MAVLINK_BRIDGE_BAUD);
  PLUART::setUseByteStreamBuffer(true);
  PLUART::setUseLineBuffer(false);
  PLUART::enable();
}

void loop(void) {
  while (PLUART::byteAvailable()) {
    process_rx_bytes(PLUART::readByte());
  }
}
