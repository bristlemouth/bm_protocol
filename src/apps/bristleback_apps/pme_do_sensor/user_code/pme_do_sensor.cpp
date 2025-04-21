#include "pme_do_sensor.h"
#include "FreeRTOS.h"
#include "bm_os.h"
#include "bm_printf.h"
#include "configuration.h"
#include "payload_uart.h"
#include "serial.h"
#include "spotter.h"
#include "stm32_rtc.h"
#include "task_priorities.h"
#include "uptime.h"
#include "user_code.h"
#include "util.h"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

#define LAST_WIPE_EPOCH_KEY "lastWipeEpochS"
//#define DOT_INTERVAL_KEY "DOTinterval"
//#define WIPE_INTERVAL_KEY "WipeInterval"

extern uint32_t pmeLogEnable;

uint32_t lastWipeEpochS = 0;
//uint32_t DOTinterval = 600;
//uint32_t WipeInterval = 14400;

//Function to request a DO measurement from the microDOT sensor
static constexpr char queryMDOT[] = "MDOT\r";
//Function to request a wipe from the microDOT sensor
static constexpr char queryWIPE[] = "WDOT\r";
//Function to request a serial number of the microdot
static constexpr char querySN[] = "SN\r";

void ledBlinkBoot() {
  for (int i = 0; i < 5; i++) {
    IOWrite(&LED_GREEN, 1);
    bm_delay(200);
    IOWrite(&LED_GREEN, 0);
    bm_delay(200);
  }
}

void ledBlinkOnce(IOPinHandle_t *led) {
  IOWrite(led, 1);
  bm_delay(100);
  IOWrite(led, 0);
  bm_delay(100);
}

void ledBlinkTwice(IOPinHandle_t *led) {
  IOWrite(led, 1);
  bm_delay(100);
  IOWrite(led, 0);
  bm_delay(100);
  IOWrite(led, 1);
  bm_delay(100);
  IOWrite(led, 0);
  bm_delay(100);
}

void ledAllOff() {
  IOWrite(&LED_BLUE, 0);
  IOWrite(&LED_GREEN, 0);
  IOWrite(&LED_RED, 0);
}

void saveLastWipeEpoch(uint32_t newLastWipeEpochS) {
  set_config_uint(BM_CFG_PARTITION_HARDWARE, LAST_WIPE_EPOCH_KEY, strlen(LAST_WIPE_EPOCH_KEY),
                  newLastWipeEpochS);
  save_config(BM_CFG_PARTITION_HARDWARE, false);
  lastWipeEpochS = newLastWipeEpochS;
}

uint32_t loadLastWipeEpoch() {
  bool success = get_config_uint(BM_CFG_PARTITION_HARDWARE, LAST_WIPE_EPOCH_KEY,
                                 strlen(LAST_WIPE_EPOCH_KEY), &lastWipeEpochS);
  if (!success) {
    printf("LAST_WIPE_EPOCH_KEY not found. Initializing to 0.\n");
    saveLastWipeEpoch(0); // Save default value
  }
  return lastWipeEpochS;
}

/**
 * @brief Initializes the PME DOT Sensor and wiper
 *
 * This function performs several operations to prepare the sensor for use:
 * - Asserts that the system configuration partition is available.
 * - Initializes the parser.
 * - Retrieves the sensor log enable configuration.
 * - Initializes the Payload UART (PLUART) with a specified task priority.
 * - Sets the baud rate for the PLUART to 115200, which is the rate expected by the Seapoint turbidity sensor.
 * - Disables the use of raw byte stream buffer in the PLUART.
 * - Enables the use of line buffer in the PLUART. Note that the PLUART only stores a single line at a time.
 *   If the attached payload sends lines faster than the app reads them, they will be overwritten and data will be lost.
 * - Sets a line termination character for the PLUART according to the protocol of the sensor.
 * - Enables the PLUART, effectively turning on the UART.
 */
void PmeSensor::init() {
  bm_delay(10000); //boot delay to allow for user to connect for viewing
  _DOTparser.init();
  _WIPEparser.init();
  lastWipeEpochS = loadLastWipeEpoch();
  ledBlinkBoot();
  ledAllOff();

  PLUART::init(USER_TASK_PRIORITY);
  // Baud set to 9600, which is expected by the microDOT sensor
  PLUART::setBaud(BAUD_RATE);
  // Disable passing raw bytes to user app.
  PLUART::setUseByteStreamBuffer(false);
  /// Warning: PLUART only stores a single line at a time. If your attached payload sends lines
  /// faster than the app reads them, they will be overwritten and data will be lost.
  PLUART::setUseLineBuffer(true);
  // Set a line termination character per protocol of the sensor.
  PLUART::setTerminationCharacter(LINE_TERM);
  // Turn on the UART.
  PLUART::enable();
  PLUART::write((uint8_t *)querySN, sizeof(querySN));
  bm_delay(250);
  if (PLUART::lineAvailable()) {
    PLUART::readLine(_SNpayload_buffer, sizeof(_SNpayload_buffer));
    printf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n"
           "~~~ Communication with microDOT established! ~~~\n"
           "~~~             S/N: %s             ~~~\n",
           _SNpayload_buffer);
    printf("~~~            Begin transmission            ~~~\n"
           "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
  } else {
    printf("!!! No microDOT S/N received - is the device connected?\n");
  }
}

/**
 * @brief Retrieves DO data from the microDOT Sensor.
 *
 * This function checks if a line of data is available from the sensor. If available, it reads the line into a buffer.
 * It then logs the data along with the current system uptime and RTC time.
 * The function then attempts to parse the data from the buffer. If the parsing is successful and the data is of the correct type,
 * it populates the passed PmeDissolvedOxygenMsg::Data structure with the parsed data and the current system time.
 *
 * @param d Reference to a PmeDissolvedOxygenMsg::Data structure where the parsed data will be stored.
 * @return Returns true if data was successfully retrieved and parsed, false otherwise.
 */
bool PmeSensor::getDoData(PmeDissolvedOxygenMsg::Data &d) {
  bool success = false;

  RTCTimeAndDate_t time_and_date = {};
  rtcGet(&time_and_date);
  char rtc_time_str[32] = {};
  rtcPrint(rtc_time_str, NULL);
  d.header.reading_time_utc_ms = rtcGetMicroSeconds(&time_and_date) / 1000;

  //Take DO measurement on boot
  PLUART::write((uint8_t *)queryMDOT, strlen(queryMDOT));
  ledBlinkOnce(&LED_BLUE);
  bm_delay(2800); //2800 to account for the 200ms delay in the LED blink function

  if (PLUART::lineAvailable()) {
    uint16_t do_read_len = PLUART::readLine(_DOTpayload_buffer, sizeof(_DOTpayload_buffer));
    ledBlinkOnce(&LED_GREEN);
    // Make spotter_log conditional on system config
    if (pmeLogEnable == 1) {
      spotter_log(0, PME_DO_RAW_LOG, USE_TIMESTAMP, "tick: %" PRIu64 ", rtc: %s, line: %.*s\n",
                  uptimeGetMs(), rtc_time_str, do_read_len, _DOTpayload_buffer);
    }
    spotter_log_console(0, "DOT | tick: %" PRIu64 ", rtc: %s, line: %.*s", uptimeGetMs(),
                        rtc_time_str, do_read_len, _DOTpayload_buffer);

    spotter_tx_data(_DOTpayload_buffer, do_read_len, BmNetworkTypeCellularIriFallback);

    printf("#  DOT | tick: %" PRIu64 ", rtc: %s, line: %.*s\n", uptimeGetMs(), rtc_time_str,
           do_read_len, _DOTpayload_buffer);

    if (_DOTparser.parseLine(_DOTpayload_buffer, do_read_len)) {
      rtcGet(&time_and_date);
      Value temp_signal = _DOTparser.getValue(2);
      Value do_signal = _DOTparser.getValue(3);
      Value q_signal = _DOTparser.getValue(4);
      if (temp_signal.type != TYPE_DOUBLE || do_signal.type != TYPE_DOUBLE ||
          q_signal.type != TYPE_DOUBLE) {
        printf("!  Parsed invalid DOT data");
      } else {
        d.header.reading_time_utc_ms = rtcGetMicroSeconds(&time_and_date) / 1000;
        d.header.reading_uptime_millis = uptimeGetMs();
        d.temperature_deg_c = temp_signal.data.double_val;
        d.do_mg_per_l = do_signal.data.double_val;
        d.quality = q_signal.data.double_val;
        // DO Sat% not available at this time; setting to NULL causes error (converting NULL to double)
        //d.do_saturation_pct = 0;
        success = true;
      }
    } else {
      printf("!  Failed to parse DOT data\n");
      ledBlinkOnce(&LED_RED);
    }
  } else {
    printf("!  No DOT line available from PLUART\n");
    ledBlinkOnce(&LED_RED);
  }
  return success;
  ledAllOff();
}

/**
 * @brief Retrieves Wipe data from the microDOT Sensor.
 *
 * This function requests a wipe and then waits for a line of data from the sensor for a set period. If available, it reads the line into a buffer.
 * It then logs the data along with the current system uptime and RTC time.
 * If data was not available, it logs a message indicating that no data was received.
 * The function then attempts to parse the data from the buffer. If the parsing is successful and the data is of the correct type,
 * it populates the passed PmeWipeMsg::Data structure with the parsed data and the current system time.
 *
 * @param w Reference to a PmeWipeMsg::Data structure where the parsed data will be stored.
 * @return Returns true if data was successfully retrieved and parsed, false otherwise.
 */
bool PmeSensor::getWipeData(PmeWipeMsg::Data &w) {
  bool success = false;

  RTCTimeAndDate_t time_and_date = {};
  rtcGet(&time_and_date);
  char rtc_time_str[32] = {};
  rtcPrint(rtc_time_str, NULL);
  w.header.reading_time_utc_ms = rtcGetMicroSeconds(&time_and_date) / 1000;

  // TODO: implement power on/off of microDOT

  PLUART::write((uint8_t *)queryWIPE, strlen(queryWIPE));
  ledBlinkTwice(&LED_BLUE);
  bm_delay(6000); // delay to ensure enough time between writing and reading
  if (PLUART::lineAvailable()) {
    uint16_t wipe_read_len = PLUART::readLine(_WIPEpayload_buffer, sizeof(_WIPEpayload_buffer));
    ledBlinkTwice(&LED_GREEN);
    printf("Wipe Read line: %s\n", _WIPEpayload_buffer);
    RTCTimeAndDate_t time_and_date = {};
    rtcGet(&time_and_date);
    char rtc_time_str[32] = {};
    rtcPrint(rtc_time_str, NULL);

    if (pmeLogEnable) {
      spotter_log(0, PME_WIPE_RAW_LOG, USE_TIMESTAMP,
                  "tick: %" PRIu64 ", rtc: %s, line: %.*s\n", uptimeGetMs(), rtc_time_str,
                  wipe_read_len, _WIPEpayload_buffer);
    }
    spotter_log_console(0, "WIPE | tick: %" PRIu64 ", rtc: %s, line: %.*s", uptimeGetMs(),
                        rtc_time_str, wipe_read_len, _WIPEpayload_buffer);
    spotter_tx_data(_WIPEpayload_buffer, wipe_read_len, BmNetworkTypeCellularIriFallback);
    printf("Wipe | tick: %" PRIu64 ", rtc: %s, line: %.*s\n", uptimeGetMs(), rtc_time_str,
           wipe_read_len, _WIPEpayload_buffer);

    if (_WIPEparser.parseLine(_WIPEpayload_buffer, wipe_read_len)) {
      printf("Parsed WIPE line: %.*s\n", wipe_read_len,
             _WIPEpayload_buffer); // Debugging; remove later
      rtcGet(&time_and_date);
      Value wipe_time = _WIPEparser.getValue(0);
      Value start1_mA = _WIPEparser.getValue(1);
      Value avg_mA = _WIPEparser.getValue(2);
      Value start2_mA = _WIPEparser.getValue(3);
      Value final_mA = _WIPEparser.getValue(4);
      Value rsource = _WIPEparser.getValue(5);
      if (wipe_time.type != TYPE_DOUBLE || start1_mA.type != TYPE_DOUBLE ||
          avg_mA.type != TYPE_DOUBLE || start2_mA.type != TYPE_DOUBLE ||
          final_mA.type != TYPE_DOUBLE || rsource.type != TYPE_DOUBLE) {
        printf("!  Parsed invalid WIPE data\n");
      } else {
        w.header.reading_time_utc_ms = rtcGetMicroSeconds(&time_and_date) / 1000;
        w.header.reading_uptime_millis = uptimeGetMs();
        w.wipe_time_sec = wipe_time.data.double_val;
        w.start1_mA = start1_mA.data.double_val;
        w.avg_mA = avg_mA.data.double_val;
        w.start2_mA = start2_mA.data.double_val;
        w.final_mA = final_mA.data.double_val;
        w.rsource = rsource.data.double_val;
        printf("#  Wipe Parse Success  >>  Epoch time: %llu, Uptime: %llu  |  Wipe time: "
               "%.1f, Start1: %.1f, Avg: %.1f, Start2: %.1f, Final: %.1f, Rsource: %.1f\n",
               w.header.reading_time_utc_ms, w.header.reading_uptime_millis, w.wipe_time_sec,
               w.start1_mA, w.avg_mA, w.start2_mA, w.final_mA, w.rsource);
        success = true;
      }
    } else {
      printf("!  Failed to parse WIPE data\n");
    }
  } else {
    printf("!  No wipe line available from PLUART\n");
    ledBlinkTwice(&LED_RED);
  }
  return success;
  ledAllOff();
}

/**
 * @brief Flushes the data from the sensor driver.
 */
void PmeSensor::flush(void) { PLUART::reset(); }
