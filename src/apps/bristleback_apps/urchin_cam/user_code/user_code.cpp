#include "user_code.h"
#include "configuration.h"
#include "payload_uart.h"
#include "spotter.h"
#include "task_priorities.h"
#include "uptime.h"

#define LED_ON_TIME_MS 20
#define LED_PERIOD_MS 7000
#define DEFAULT_BAUD_RATE 9600
#define DEFAULT_LINE_TERM 10 // newline, '\n', 0x0A
#define MAX_READINGS_TO_TX 64
#define SPOTTER_TX_SECONDS 30

#define BAUD_CFG_KEY "plUartBaudRate"
#define LINE_TERM_CFG_KEY "plUartLineTerm"
#define LOG_ENABLE_CFG_KEY "sensorBmLogEnable"

// variables to store configurations retrieved from NVM
static uint32_t baud_rate_config = DEFAULT_BAUD_RATE;
static uint32_t line_term_config = DEFAULT_LINE_TERM;
static uint32_t bm_log_enable = false;

static uint64_t last_uart_rx_uptime = 0;
static uint64_t last_spotter_tx_uptime = 0;
static uint8_t readings[MAX_READINGS_TO_TX] = {};
static size_t reading_index = 0;

static void handle_spotter_tx(void) {
  uint64_t now = uptimeGetMs();
  const bool been_long_enough = now - last_spotter_tx_uptime >= SPOTTER_TX_SECONDS;
  const bool buffer_is_full = reading_index >= MAX_READINGS_TO_TX;
  const bool should_tx = been_long_enough || buffer_is_full;
  if (should_tx) {
    BmErr err = spotter_tx_data(&readings[0], reading_index, BmNetworkTypeCellularOnly);
    if (err == BmOK) {
      last_spotter_tx_uptime = now;
      reading_index = 0;
    }
  }
}

static void handle_uart_rx(void) {
  const bool buffer_space_available = reading_index < MAX_READINGS_TO_TX;
  while (PLUART::byteAvailable() && buffer_space_available) {
    uint8_t num_urchins_detected = PLUART::readByte();
    readings[reading_index++] = num_urchins_detected;
    last_uart_rx_uptime = uptimeGetMs();
  }
}

static void handle_leds(void) {
  const uint64_t now = uptimeGetMs();

  // Blink the LED green periodically for sign of life
  static uint64_t led1_last_on = now;
  const bool led1_should_turn_on = now - led1_last_on >= LED_PERIOD_MS;
  const bool led1_should_turn_off =
      !led1_should_turn_on && now - led1_last_on >= LED_ON_TIME_MS;
  if (led1_should_turn_on) {
    IOWrite(&LED_GREEN, 1);
    led1_last_on = now;
  } else if (led1_should_turn_off) {
    IOWrite(&LED_GREEN, 0);
  }

  // Blink the LED blue when we receive data on the uart
  const bool led2_should_be_on = now - last_uart_rx_uptime < LED_ON_TIME_MS;
  IOWrite(&LED_BLUE, led2_should_be_on);
}

void setup(void) {
  get_config_uint(BM_CFG_PARTITION_USER, BAUD_CFG_KEY, strlen(BAUD_CFG_KEY), &baud_rate_config);
  get_config_uint(BM_CFG_PARTITION_USER, LINE_TERM_CFG_KEY, strlen(LINE_TERM_CFG_KEY),
                  &line_term_config);
  get_config_uint(BM_CFG_PARTITION_SYSTEM, LOG_ENABLE_CFG_KEY, strlen(LOG_ENABLE_CFG_KEY),
                  &bm_log_enable);

  PLUART::init(USER_TASK_PRIORITY);
  PLUART::setBaud(baud_rate_config);
  PLUART::setUseByteStreamBuffer(true);
  PLUART::setUseLineBuffer(false);
  PLUART::setTerminationCharacter((char)line_term_config);
  PLUART::enable();

  IOWrite(&BB_VBUS_EN, 0);
  vTaskDelay(pdMS_TO_TICKS(5));
  IOWrite(&BB_PL_BUCK_EN, 0);

  IOWrite(&LED_BLUE, 0);
  IOWrite(&LED_GREEN, 0);
  IOWrite(&LED_RED, 0);
}

void loop(void) {
  handle_spotter_tx();
  handle_uart_rx();
  handle_leds();
}
