#include "bristlefin.h"
#include "debug.h"

namespace BF {

  // Power rail control functions
  void enableVbus() {
    IOWrite(&VBUS_BF_EN, 0); // 0 enables, 1 disables. Needed for VOUT and 5V.
  }
  void disableVbus() {
    IOWrite(&VBUS_BF_EN, 1); // 0 enables, 1 disables. Needed for VOUT and 5V.
  }
  void enable5V() {
    IOWrite(&BF_5V_EN, 0); // 0 enables, 1 disables. Needed for SDI12 and RS485.
  }
  void disable5V() {
    IOWrite(&BF_5V_EN, 1); // 0 enables, 1 disables. Needed for SDI12 and RS485.
  }
  void enableVout() {
    IOWrite(&BF_PL_BUCK_EN, 0); // 0 enables, 1 disables. Vout
  }
  void disableVout() {
    IOWrite(&BF_PL_BUCK_EN, 1); // 0 enables, 1 disables. Vout
  }
  void enable3V() {
    IOWrite(&BF_3V3_EN, 1); // 1 enables, 0 disables. Needed for I2C and I/O control.
  }
  void disable3V() {
    IOWrite(&BF_3V3_EN, 0); // 1 enables, 0 disables. Needed for I2C and I/O control.
  }

  void setLed(uint8_t led_num, led_state_e led_state) {
    // Error handling: check if led_num is valid
    if (led_num < 1 || led_num > 2) {
      printf("Invalid LED number. Choose between 1 and 2.\n");
      return;
    }

    // Determine which LED state to set based on led_state
    switch (led_state) {
      case LED_GREEN:
        if (led_num == 1) {
          IOWrite(&BF_LED_G1, BF_LED_ON);
          IOWrite(&BF_LED_R1, BF_LED_OFF);
        }
        else {
          IOWrite(&BF_LED_G2, BF_LED_ON);
          IOWrite(&BF_LED_R2, BF_LED_OFF);
        }
        break;

      case LED_RED:
        if (led_num == 1) {
          IOWrite(&BF_LED_G1, BF_LED_OFF);
          IOWrite(&BF_LED_R1, BF_LED_ON);
        }
        else {
          IOWrite(&BF_LED_G2, BF_LED_OFF);
          IOWrite(&BF_LED_R2, BF_LED_ON);
        }
        break;

      case LED_YELLOW:
        if (led_num == 1) {
          IOWrite(&BF_LED_G1, BF_LED_ON);
          IOWrite(&BF_LED_R1, BF_LED_ON);
        }
        else {
          IOWrite(&BF_LED_G2, BF_LED_ON);
          IOWrite(&BF_LED_R2, BF_LED_ON);
        }
        break;

      case LED_OFF:
        if (led_num == 1) {
          IOWrite(&BF_LED_G1, BF_LED_OFF);
          IOWrite(&BF_LED_R1, BF_LED_OFF);
        }
        else {
          IOWrite(&BF_LED_G2, BF_LED_OFF);
          IOWrite(&BF_LED_R2, BF_LED_OFF);
        }
        break;
    }
  }

} // namespace BF
