#include "user_code.h"
#include "motor_process.h"
#include "pwm_debug.h"
#include "uptime.h"

void setup(void) {
  lpmPeripheralActive(LPM_BOOT);
  IOWrite(&BB_VBUS_EN, 0);
  IOWrite(&BB_PL_BUCK_EN, 0);
  pwm_debug_init();
  motor_init();
}

void loop(void) {
  static uint32_t loop_time_ms = uptimeGetMs();
  static bool forward = true;
  static bool rest = false;

  if (uptimeGetMs() - loop_time_ms >= 10000) {
    if (rest) {
      set_motor_state(MOTOR_OFF);
      rest = false;
    } else if (forward) {
      set_motor_state(MOTOR_FORWARD);
      forward = false;
      rest = true;
    } else {
      set_motor_state(MOTOR_BACKWARD);
      forward = true;
      rest = true;
    }
    loop_time_ms = uptimeGetMs();
  }
}
