#include "user_code.h"
#include "bsp.h"
#include "io.h"
#include "serial_bridge.h"
#include <string.h>

void setup() {
  serial_bridge_init();
  IOWrite(&BB_VBUS_EN, 0);
  // ensure Vbus stable before enable Vout with a 5ms delay.
  vTaskDelay(pdMS_TO_TICKS(5));
  // enable Vout, 12V by default.
  IOWrite(&BB_PL_BUCK_EN, 0);
}

void loop() { serial_bridge_handle(); }
