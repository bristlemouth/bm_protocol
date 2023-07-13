#pragma once

#include "gpio.h"
#include "bsp.h"

// I/O states for LED1 and LED2 elements.
#define BF_LED_ON (0)
#define BF_LED_OFF (1)

namespace BF {

  typedef enum {
    LED_OFF = 0,
    LED_GREEN,
    LED_RED,
    LED_YELLOW
  } led_state_e;

  // Power rail control functions
  void enableVbus();
  void disableVbus();
  void enable5V();
  void disable5V();
  void enableVout();
  void disableVout();
  void enable3V();
  void disable3V();

  // LED control functions
  void setLed(uint8_t led_num, led_state_e led_state);

} // namespace BF
