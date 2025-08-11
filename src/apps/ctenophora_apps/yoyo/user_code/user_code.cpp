#include "user_code.h"
#include "bm_config.h"
#include "motor_process.h"
#include "pubsub.h"
#include "pwm_debug.h"
#include "uptime.h"
#include <string.h>

#define POWER_SAMPLER_PERIOD_MS (100)

#define MOTOR_CONTROL_TOPIC "motor"

void motor_sub(uint64_t node_id, const char *topic, uint16_t topic_len, const uint8_t *data,
               uint16_t data_len, uint8_t type, uint8_t version) {
  (void)node_id;
  (void)type;
  (void)version;
  const char *command = (const char *)data;

  bm_debug("Got data on topic: %.*s\n", topic_len, topic);

  if (command) {
    if (strncmp("forward", command, data_len) == 0) {
      bm_debug("Setting motor forward\n");
      set_motor_state(MOTOR_FORWARD);
    } else if (strncmp("backward", command, data_len) == 0) {
      set_motor_state(MOTOR_BACKWARD);
      bm_debug("Setting motor backward\n");
    } else if (strncmp("off", command, data_len) == 0) {
      set_motor_state(MOTOR_OFF);
      bm_debug("Turning motor off\n");
    }
  }
}

//static void back_forth(void) {
//  static uint32_t loop_time_ms = uptimeGetMs();
//  static bool forward = true;
//  static bool rest = false;
//
//  if (uptimeGetMs() - loop_time_ms >= 10000) {
//    if (rest) {
//      set_motor_state(MOTOR_OFF);
//      rest = false;
//    } else if (forward) {
//      set_motor_state(MOTOR_FORWARD);
//      forward = false;
//      rest = true;
//    } else {
//      set_motor_state(MOTOR_BACKWARD);
//      forward = true;
//      rest = true;
//    }
//    loop_time_ms = uptimeGetMs();
//  }
//}

void setup(void) {
  lpmPeripheralActive(LPM_BOOT);

  pwm_debug_init();
  motor_init();

  bm_sub(MOTOR_CONTROL_TOPIC, motor_sub);

  // Enable power to bristleback and the motor
  IOWrite(&BB_VBUS_EN, 0);
  IOWrite(&BB_PL_BUCK_EN, 0);
}

void loop(void) {}
