#include "user_code.h"
#include "bm_network.h"
#include "bm_printf.h"
#include "bm_pubsub.h"
// #include "bristlefin.h"
#include "bsp.h"
#include "debug.h"
#include "lwip/inet.h"
#include "sensors.h"
#include "stm32_rtc.h"
#include "task_priorities.h"
#include "uptime.h"
#include "util.h"

#include "debug_configuration.h"

#define LED_ON_TIME_MS 20
#define LED_PERIOD_MS 1000

// app_main passes a handle to the user config partition in NVM.
extern cfg::Configuration *systemConfigurationPartition;

static uint32_t tx_pin_state = 0;
static uint32_t rx_pin_state = 0;

static constexpr char s_tx_pin_state[] = "txPinState";
static constexpr char s_rx_pin_state[] = "rxPinState";


void setup(void) {
  /* USER ONE-TIME SETUP CODE GOES HERE */
  configASSERT(systemConfigurationPartition);
  systemConfigurationPartition->getConfig(s_tx_pin_state, strlen(s_tx_pin_state), tx_pin_state);
  systemConfigurationPartition->getConfig(s_rx_pin_state, strlen(s_rx_pin_state), rx_pin_state);
}

void loop(void) {
  /* USER LOOP CODE GOES HERE */

  static uint32_t tx_pin_state_prev = 0;
  static uint32_t rx_pin_state_prev = 0;

  // Turn LED1 on green every LED_PERIOD_MS milliseconds.
  systemConfigurationPartition->getConfig(s_tx_pin_state, strlen(s_tx_pin_state), tx_pin_state);
  systemConfigurationPartition->getConfig(s_rx_pin_state, strlen(s_rx_pin_state), rx_pin_state);


  if (tx_pin_state != tx_pin_state_prev) {
    IOWrite(&PAYLOAD_TX, tx_pin_state);
    tx_pin_state_prev = tx_pin_state;
  }
  if (rx_pin_state != rx_pin_state_prev) {
    IOWrite(&PAYLOAD_TX, rx_pin_state);
    rx_pin_state_prev = rx_pin_state;
  }

}
