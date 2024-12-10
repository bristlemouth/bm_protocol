#include "user_code.h"
#include "bristlefin.h"
#include "bsp.h"
#include "debug.h"
#include "lwip/inet.h"
#include "payload_uart.h"
#include "rx_live_sensor.h"
#include "sensors.h"
#include "spotter.h"
#include "stm32_rtc.h"
#include "task_priorities.h"
#include "uptime.h"
#include "usart.h"
#include "util.h"

#define LED_ON_TIME_MS 20
#define LED_PERIOD_MS 1000
// NOTE - we manually set our Rx-LIVE to 115200 in a one-time setup.
#define DEFAULT_BAUD_RATE 115200
// NOTE - the Rx-LIVE uses Carriage Return for line term for incoming commands.
// It will terminate response lines with \r\n
// We're just using the first \r char, so the next line will have the \n at the start.
/// TODO - allow configuration of different Tx and Rx line terminations.
/// TODO - allow for multi-charater terminations.
#define DEFAULT_LINE_TERM 13 // CR / '\r', 0x0D

// Size of the structure we'll use to report detection counts for each unique tag.
#define RX_LIVE_DETECTION_SIZE sizeof(TagID)
// We'll store up to 300 bytes worth of tags, this ends up being 27 currently.
#define MAX_TAGS (uint16_t)(300 / RX_LIVE_DETECTION_SIZE)
// We'll add another uint32_t to our report for the Status Lines
#define MAX_TX_BUFFER_SIZE (MAX_TAGS * RX_LIVE_DETECTION_SIZE + sizeof(uint32_t))
// Sample for 3 minutes, then report all tag detections and Status Lines
#define DEFAULT_SAMPLE_DUR_S 180

// The serial number of our Rx-Live receiver, loaded from user config partition.
uint32_t rx_live_serial_number = 0;
static RxLiveSensor rx_live;           // Class instance for our Rx-Live.
uint32_t rx_live_sts_count = 0;        // Counter for status updates received in the session
uint32_t unique_tag_ids = 0;           // Number of unique tag IDs we've seen this session
TagID tag_detections[MAX_TAGS] = {};   // Array to store unique Tag IDs + detection counts
uint8_t tx_buffer[MAX_TX_BUFFER_SIZE]; // Buffer for data transmission
uint32_t sample_duration_s =
    DEFAULT_SAMPLE_DUR_S; // Sample duration loaded from user config if set
uint32_t sample_duration_ms = sample_duration_s * 1000;
uint32_t _sample_timer_ms = 0; // Timer variable to trigger a report

// A timer variable we can set to trigger a pulse on LED2 when we get payload serial data
static int32_t ledLinePulse = -1;

// A buffer for our Rx data from the payload uart
char payload_buffer[2048];
static u_int32_t baud_rate_config = DEFAULT_BAUD_RATE;
static u_int32_t line_term_config = DEFAULT_LINE_TERM;

/**
 * @brief Reports the count of detections per unique tag, as well as total count of Status Lines
 * received from the Rx-LIVE.
 *
 * It logs this data to Spotter SD card and
 * reports it over tx-data telemetry.
 *
 * We expect 1 Status Line at boot and then 1 per minute. The Status
 * Line count gives us an overall indication the sensor was connected and working as expected
 * during the entire sample duration.
 *
 */
void report_detections() {
  memset(tx_buffer, 0, sizeof(tx_buffer)); //  Clear the telemetry data buffer.
  memcpy(
      tx_buffer, &rx_live_sts_count,
      sizeof(rx_live_sts_count)); // Copy rx_live_sts_count into the first 4 bytes of tx_buffer.
  size_t tx_len =
      sizeof(rx_live_sts_count); // Keep track of the total number of valid bytes in the buffer.
  // Copy all tag_detections into the buffer.
  for (size_t i = 0; i < unique_tag_ids; ++i) {
    memcpy(tx_buffer + tx_len, &tag_detections[i], sizeof(TagID));
    tx_len += sizeof(TagID);
  }

  // Get the datetime from the RTC if available, useful for logging
  RTCTimeAndDate_t time_and_date = {};
  rtcGet(&time_and_date);
  char rtcTimeBuffer[32];
  rtcPrint(rtcTimeBuffer, &time_and_date);

  // Print hex string of the buffer to the console for debugging.
  static char tx_hex[MAX_TX_BUFFER_SIZE * 2 + 1];
  size_t i;
  for (i = 0; i < tx_len; ++i) {
    sprintf(&tx_hex[i * 2], "%02X", tx_buffer[i]);
  }
  tx_hex[i * 2] = '\0'; // Null-terminate the string
  spotter_log_console(0, "[rx-live-agg] detection buffer to send | tick: %llu, rtc: %s, buff: 0x%s",
            uptimeGetMs(), rtcTimeBuffer, tx_hex);
  printf("[rx-live-agg] detection buffer to send | tick: %llu, rtc: %s, buff: %s\n",
         uptimeGetMs(), rtcTimeBuffer, tx_hex);
  // Transmit over Spotter celluar or Iridium SBD fallback.
  if (spotter_tx_data(tx_buffer, tx_len, BmNetworkTypeCellularIriFallback)) {
    printf("%llut - %s | Sucessfully sent Spotter transmit data request\n", uptimeGetMs(),
           rtcTimeBuffer);
  } else {
    printf("%llut - %s | Failed to send Spotter transmit data request\n", uptimeGetMs(),
           rtcTimeBuffer);
  }
  // Reset the counts and the tag detections array for the next collection period.
  rx_live_sts_count = 0;
  unique_tag_ids = 0;
  memset(tag_detections, 0, sizeof(tag_detections));
}

/**
 * @brief Tally the tag detections and update detection counts and buffer.
 *
 * This function searches the current list of detected tags to find a match for the
 * given `latest_detection`. If a match is found, the detection count is incremented.
 * Otherwise, the new detection is added to the list, and the list is printed.
 *
 * @param latest_detection The latest tag detection to be tallied.
 */
void tally_detection(const TagID &latest_detection) {
  bool found = false;
  // Iterate through existing tag detections to find a match
  for (uint32_t i = 0; i < unique_tag_ids; ++i) {
    if (tag_detections[i].tag_serial_no == latest_detection.tag_serial_no &&
        tag_detections[i].code_freq == latest_detection.code_freq &&
        tag_detections[i].code_channel == latest_detection.code_channel) {
      // Match found; increment detection count
      if (tag_detections[i].detection_count < UINT16_MAX) {
        tag_detections[i].detection_count++;
      } else {
        // Print an error for debugging
        printf(
            "Error: Detection count for tag serial no %u has reached its maximum value (%u).\n",
            tag_detections[i].tag_serial_no, UINT16_MAX);
      }
      found = true;
      break;
    }
  }
  // No match found; add the latest detection as a new entry
  if (!found) {
    if (unique_tag_ids < MAX_TAGS) {
      tag_detections[unique_tag_ids] = latest_detection;
      tag_detections[unique_tag_ids].detection_count = 1; // Initialize count to 1
      unique_tag_ids++;
    } else {
      // Print an error for debugging if no more room for new tags
      printf("Error: Maximum number of unique tags (%d) reached.\n", MAX_TAGS);
      return;
    }
  }

  // Nicely print the whole list of tag detections to the console
  printf("Current Tag Detections:\n");
  printf("---------------------------------------------------------\n");
  printf("| %-5s | %-10s | %-6s | %-8s | %-5s | %-10s |\n", "Index", "Serial No", "Char",
         "Freq", "Chan", "Detections");
  printf("---------------------------------------------------------\n");
  for (uint32_t i = 0; i < unique_tag_ids; ++i) {
    printf("| %-5u | %-10u | %-6c | %-8u | %-5u | %-10u |\n", i,
           tag_detections[i].tag_serial_no, tag_detections[i].code_char,
           tag_detections[i].code_freq, tag_detections[i].code_channel,
           tag_detections[i].detection_count);
  }
  printf("---------------------------------------------------------\n");
}

void setup() {
  /* USER ONE-TIME SETUP CODE GOES HERE */

  // Baud rate for the payload UART interface
  get_config_uint(BM_CFG_PARTITION_USER, "plUartBaudRate", strlen("plUartBaudRate"),
                  &baud_rate_config);
  // Line termination character for the payload UART interface
  get_config_uint(BM_CFG_PARTITION_USER, "plUartLineTerm", strlen("plUartLineTerm"),
                  &line_term_config);
  // The serial number of the connected Rx-LIVE receiver.
  get_config_uint(BM_CFG_PARTITION_USER, "rxLiveSerial", strlen("rxLiveSerial"),
                  &rx_live_serial_number);
  // The sample duration in seconds, at which we'll report all tag detection and Status Line counts
  get_config_uint(BM_CFG_PARTITION_USER, "sampleDurS", strlen("sampleDurS"),
                  &sample_duration_s);

  sample_duration_ms = sample_duration_s * 1000;
  _sample_timer_ms = uptimeGetMs();

  // Setup the UART – the on-board serial driver that talks to the RS485 transceiver.
  bristlefin.setRS485HalfDuplex();   // Set the RS485 transceiver to Half-Duplex.
  PLUART::init(USER_TASK_PRIORITY);  // Initialize the UART FreeRTOS task.
  PLUART::setBaud(baud_rate_config); // Baud set per expected baud rate of the sensor.
  // Setup PLUART transactions for proper RS485 enable Tx / enable Rx
  PLUART::enableTransactions(Bristlefin::setRS485Tx, Bristlefin::setRS485Rx);
  PLUART::setUseLineBuffer(true); // Enable PLUART line parsing
  /// Warning: PLUART only stores a single line at a time. If your attached payload sends lines
  /// faster than the app reads them, they will be overwritten and data will be lost.
  // Set a line termination character per protocol of the sensor.
  PLUART::setTerminationCharacter((char)line_term_config);
  PLUART::enable(); // Turn on the UART.

  // Turn on the Rx-Live sensor power
  bristlefin.enableVbus();      // Enable the input to the Vout power supply.
  vTaskDelay(pdMS_TO_TICKS(5)); // ensure Vbus stable before enable Vout with a 5ms delay.
  bristlefin.enableVout();      // enable Vout to turn on the Rx-Live, 12V by default.
  // setup the RX-Live interface
  rx_live.init(rx_live_serial_number);
  // Must send an RTMNOW command to enable data output in Profile 0
  //  see section 2.6.1.2.1: 'Profile O will not commence until an RTMNOW or RTM485 command has been issued.'
  rx_live.sendCommand(RX_LIVE_RTMNOW);
  vTaskDelay(pdMS_TO_TICKS(150));
  printf("tx_buffer %u bytes, can store %u tags.\n", MAX_TAGS * RX_LIVE_DETECTION_SIZE,
         MAX_TAGS);
}

void loop() {
  /* USER LOOP CODE GOES HERE */
  // Read a line if it is available
  // Note - PLUART::setUseLineBuffer must be set true in setup to enable lines.
  if (PLUART::lineAvailable()) {
    uint16_t read_len = PLUART::readLine(payload_buffer, sizeof(payload_buffer));

    // Get the RTC time (if available) for use in log outputs
    RTCTimeAndDate_t time_and_date = {};
    rtcGet(&time_and_date);
    char rtcTimeBuffer[32];
    rtcPrint(rtcTimeBuffer, &time_and_date);

    // Print the payload data to a Spotter SD file, to the Spotter console, and to the printf console.
    spotter_log(0, "rx_live_raw.log", USE_TIMESTAMP, "tick: %llu, rtc: %s, line: %.*s\n",
               uptimeGetMs(), rtcTimeBuffer, read_len, payload_buffer);
    spotter_log_console(0, "[rx-live-raw] | tick: %llu, rtc: %s, line: %.*s", uptimeGetMs(),
              rtcTimeBuffer, read_len, payload_buffer);
    printf("[rx-live-raw] | tick: %llu, rtc: %s, line: %.*s\n", uptimeGetMs(), rtcTimeBuffer,
           read_len, payload_buffer);

    // pass the line into the Rx-LIVE state machine and parser.
    RxLiveStatusCode rx_code = rx_live.run((const uint8_t *)payload_buffer, read_len);
    // If we receive a return code that indicates an interesting state change or error, log it for testing and debugging.
    if (rx_code > RX_CODE_INFO) {
      spotter_log(0, "rx_live_state.log", USE_TIMESTAMP, "tick: %llu, rtc: %s, rx_code: %d\n",
                 uptimeGetMs(), rtcTimeBuffer, rx_code);
      spotter_log_console(0, "[rx-live-state] | tick: %llu, rtc: %s, rx_code: %d", uptimeGetMs(),
                rtcTimeBuffer, rx_code);
      printf("[rx-live-state] | tick: %llu, rtc: %s, rx_code: %d\n", uptimeGetMs(),
             rtcTimeBuffer, rx_code);
    }
    if (rx_code == RX_CODE_DETECTION) {
      // If we received a Tag Detection Line, update our tag counts buffer.
      printf("[rx-live-state] :: DETECTION!\n");
      TagID latest_detection = rx_live.getLatestDetection();
      tally_detection(latest_detection);
    } else if (rx_code == RX_CODE_STS) {
      // If we received a Status Line, update our status line count.
      // We should expect to see 1 of these per minute if everything is working as expected.
      rx_live_sts_count++;
      printf("[rx-live-state] :: %lu STS polls received in session.\n", rx_live_sts_count);
    }
    ledLinePulse = uptimeGetMs(); // trigger a pulse on LED2
  } else {                        // No new UART Rx line available.
    // Run RxLive state machine without Rx data to service the state machine timers
    RxLiveStatusCode rx_code = rx_live.run(nullptr, 0);
    // An interesting state change or an error
    if (rx_code > RX_CODE_INFO) {
      // Get the RTC time (if available) for use in log outputs
      RTCTimeAndDate_t time_and_date = {};
      rtcGet(&time_and_date);
      char rtcTimeBuffer[32];
      rtcPrint(rtcTimeBuffer, &time_and_date);
      // Log the state transition or error to Spotter SD card and USB console.
      spotter_log(0, "rx_live_state.log", USE_TIMESTAMP, "tick: %llu, rtc: %s, rx_code: %d\n",
                 uptimeGetMs(), rtcTimeBuffer, rx_code);
      spotter_log_console(0, "[rx-live-state] | tick: %llu, rtc: %s, rx_code: %d", uptimeGetMs(),
                rtcTimeBuffer, rx_code);
      printf("[rx-live-state] | tick: %llu, rtc: %s, rx_code: %d\n", uptimeGetMs(),
             rtcTimeBuffer, rx_code);
    }
  } // \else

  if ((u_int32_t)uptimeGetMs() - _sample_timer_ms >= sample_duration_ms) {
    printf("[rx-live-state] :: GENERATING SAMPLE!\n");
    report_detections();
    _sample_timer_ms = uptimeGetMs();
  }

  /* LED TIMER CONTROL BELOW HERE */
  static bool led2State = false;
  /// This checks for a trigger set by ledLinePulse when data is received from the payload UART.
  ///   Each time this happens, we pulse LED2 Green.
  // If LED2 is off and the ledLinePulse flag is set (done in our payload UART process line function), turn it on Green.
  if (!led2State && ledLinePulse > -1) {
    bristlefin.setLed(2, Bristlefin::LED_GREEN);
    led2State = true;
  }
  // If LED2 has been on for LED_ON_TIME_MS, turn it off.
  else if (led2State && ((u_int32_t)uptimeGetMs() - ledLinePulse >= LED_ON_TIME_MS)) {
    bristlefin.setLed(2, Bristlefin::LED_OFF);
    ledLinePulse = -1;
    led2State = false;
  }

  /// This section demonstrates a simple non-blocking bare metal method for rollover-safe timed tasks,
  ///   like blinking an LED.
  /// More canonical (but more arcane) modern methods of implementing this kind functionality
  ///   would bee to use FreeRTOS tasks or hardware timer ISRs.
  static u_int32_t ledPulseTimer = uptimeGetMs();
  static u_int32_t ledOnTimer = 0;
  static bool led1State = false;
  // Turn LED1 on green every LED_PERIOD_MS milliseconds.
  if (!led1State && ((u_int32_t)uptimeGetMs() - ledPulseTimer >= LED_PERIOD_MS)) {
    bristlefin.setLed(1, Bristlefin::LED_GREEN);
    ledOnTimer = uptimeGetMs();
    ledPulseTimer += LED_PERIOD_MS;
    led1State = true;
  }
  // If LED1 has been on for LED_ON_TIME_MS milliseconds, turn it off.
  else if (led1State && ((u_int32_t)uptimeGetMs() - ledOnTimer >= LED_ON_TIME_MS)) {
    bristlefin.setLed(1, Bristlefin::LED_OFF);
    led1State = false;
  }
} // \loop()
