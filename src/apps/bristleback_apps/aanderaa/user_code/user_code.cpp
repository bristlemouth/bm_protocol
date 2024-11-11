#include "user_code.h"
#include "OrderedKVPLineParser.h"
#include "aanderaa_data_msg.h"
#include "app_util.h"
#include "array_utils.h"
#include "avgSampler.h"
#include "bsp.h"
#include "debug.h"
#include "device_info.h"
#include "lwip/inet.h"
#include "payload_uart.h"
#include "pubsub.h"
#include "sensorWatchdog.h"
#include "sensors.h"
#include "spotter.h"
#include "stm32_rtc.h"
#include "task_priorities.h"
#include "uptime.h"
#include "usart.h"

#define LED_ON_TIME_MS 20
#define LED_PERIOD_MS 1000

#define AANDERAA_WATCHDOG_MAX_TRIGGERS (3)
static constexpr char AANDERAA_WATCHDOG_ID[] = "Aanderaa";
static constexpr char AANDERAA_RAW_LOG[] = "aanderaa_raw.log";
static constexpr char s_current_agg_period_min[] = "currentAggPeriodMin";
static constexpr char s_n_skip_readings[] = "nSkipReadings";
static constexpr char s_reading_interval_ms[] = "readingIntervalMs";
static constexpr char s_payload_wd_to_s[] = "payloadWdToS";
static constexpr char s_pl_uart_baud_rate[] = "plUartBaudRate";
static constexpr char s_mfg_tx_test_enable[] = "mfgTxTestModeEnable";
static constexpr char s_sensor_bm_log_enable[] = "sensorBmLogEnable";

// Enable logging of sensor data to the BM log.
static uint32_t sensorBmLogEnable = 0;

// How long to hold Aanderaa off when attempting FTL recovery.
// -- Measured 1 second via trial & error and scope grabs: https://www.notion.so/sofarocean/FTL-recovery-works-Manually-stop-aanderaa-to-test-edb988cab3a3467caf2aab9f980fcb11
// -- So setting 2 seconds for safety factor
#define AANDERAA_RESET_TIME_MS (2000)
// How long to wait after turn-off before Tx tickling
// -- Measured 50ms via scope grabs, setting 100ms
#define AANDERAA_TX_TICKLE_DELAY (100)
// Use a No-op ";" command to tickle the Aanderaa
static const char tx_tickle_data[] = ";\r\n";

// Config flag to enable a tx test for manufacturing.
static uint32_t mfg_tx_test_enable = 0;
#define MFG_TEST_TX_PERIOD_MS (30 * 1000)

/// Default mote configurations and local variables
// How many minutes to collect readings for before shipping an aggregation.
#define DEFAULT_CURRENT_AGG_PERIOD_MIN (0)
static float current_agg_period_min = DEFAULT_CURRENT_AGG_PERIOD_MIN;
// inline helper to turn this into milliseconds
#define CURRENT_AGG_PERIOD_MS ((double)current_agg_period_min * 60 * 1000)

// How many samples to leave out of aggregation during sensor initialization.
// - If using reading intervals <5 seconds, recommend 2, otherwise 0.
// It takes time for some internal Aanderaa init, and we're not parsing the "*" characters to infer errors yet.
#define DEFAULT_N_SKIP_READINGS (0)
static uint32_t n_skip_readings = DEFAULT_N_SKIP_READINGS;
static uint8_t readings_skipped = 0;

// The reading interval in ms the Aanderaa is configured for.
// - Currently, must take care to manually set Aanderaa and this setting to the same values.
#define DEFAULT_CURRENT_READING_INTERVAL_MS (60 * 1000)
static uint32_t reading_interval_ms = DEFAULT_CURRENT_READING_INTERVAL_MS;

// Baud rate for the Payload UART interface
// - Currently, must take care to manually set Aanderaa and this setting to the same values.
#define DEFAULT_BAUD_RATE 9600
static uint32_t baud_rate = DEFAULT_BAUD_RATE;

// Extra sample padding to account for timing slop. Not currently configurable.
#define N_SAMPLES_PAD 10

// Set timeout in seconds after which a power cycle will be attempted to recover the attached payload
// -- Intent is to recover from FTL (Failure To Launch) errors.
// -- This needs to be set with consideration of the configured Aanderaa reading interval.
#define DEFAULT_PAYLOAD_WATCHDOG_TIMEOUT_S (70)
static uint32_t payload_wd_to_s = DEFAULT_PAYLOAD_WATCHDOG_TIMEOUT_S;
// inline helper to turn this into milliseconds
#define PAYLOAD_WATCHDOG_TIMEOUT_MS (payload_wd_to_s * 1000)

// Initialized in Setup after configs are set from PROM.
static uint64_t max_readings_in_agg = 0;
#ifdef FAKE_AANDERAA
static void spoof_aanderaa(void);
#endif // FAKE_AANDERAA

/// For Turning Text Into Numbers
#ifdef AANDERAA_BLUE
// Need to manually adjust current_data, valueTypes and keys when changing
#define NUM_PARAMS_TO_AGG 14
#else
// Need to manually adjust current_data, valueTypes and keys when changing
#define NUM_PARAMS_TO_AGG 13
#endif

typedef struct {
  uint16_t sample_count;
  float min;
  float max;
  float mean;
  float stdev;
} __attribute__((__packed__)) currentData_t;
// create a working instance here, we'll allocate memory for values later.
static AveragingSampler current_data[NUM_PARAMS_TO_AGG];
static char *stats_print_buffer; // Buffer to store debug print data for the stats aggregation.
static RTCTimeAndDate_t statsEndRtc =
    {}; // Timestamp that tracks the end of aggregation periods.

/*
 * Setup a LineParser to turn the ASCII serial data from the Aanderaa into numbers
 * Lines from the Aanderaa sensor look like:
 *    TODO - detail */
static const char lineHeader[] = "MEASUREMENT";
// Let's use 64 bit doubles for everything.
//   We've got luxurious amounts of RAM on this chip, and it's much easier to avoid roll-overs and precision issues
//   by using it vs. troubleshooting them because we prematurely optimized things.
static const ValueType valueTypes[NUM_PARAMS_TO_AGG] = {
    TYPE_DOUBLE, TYPE_DOUBLE, TYPE_DOUBLE, TYPE_DOUBLE, TYPE_DOUBLE, TYPE_DOUBLE, TYPE_DOUBLE,
    TYPE_DOUBLE, TYPE_DOUBLE, TYPE_DOUBLE, TYPE_DOUBLE, TYPE_DOUBLE, TYPE_DOUBLE,
#ifdef AANDERAA_BLUE
    TYPE_DOUBLE
#endif
};
static const char *keys[] = {"Abs Speed[cm/s]",
                             "Direction[Deg.M]",
                             "North[cm/s]",
                             "East[cm/s]",
                             "Heading[Deg.M]",
                             "Tilt X[Deg]",
                             "Tilt Y[Deg]",
                             "SP Std[cm/s]",
                             "Ping Count",
                             "Abs Tilt[Deg]",
                             "Max Tilt[Deg]",
                             "Std Tilt[Deg]",
                             "Temperature[DegC]"
#ifdef AANDERAA_BLUE
                             "Memory Used[Bytes]"
#endif
};

typedef enum AadneraaKeysIdx {
  ABS_SPEED,
  DIRECTION,
  NORTH,
  EAST,
  HEADING,
  TILT_X,
  TILT_Y,
  SP_STD,
  PING_COUNT,
  ABS_TILT,
  MAX_TILT,
  STD_TILT,
  TEMP,
} AadneraaKeysIdx_e;

static char payload_buffer[2048];
static char aanderaaTopic[BM_TOPIC_MAX_LEN];
static int aanderaaTopicStrLen;

// Declare the parser here with separator, buffer length, value types array, and number of values per line.
//   We'll initialize the parser later in setup to allocate all the memory we'll need.
static OrderedKVPLineParser parser("\t", 750, valueTypes, NUM_PARAMS_TO_AGG, keys, lineHeader);

// A timer variable we can set to trigger a pulse on LED2 when we get payload serial data
static int32_t ledLinePulse = -1;

static currentData_t aggregateStats(AveragingSampler &sampler) {
  currentData_t ret_stats = {};
  if (sampler.getNumSamples() > 0) {
    ret_stats.mean = sampler.getMean();
    ret_stats.stdev = sampler.getStd(ret_stats.mean);
    ret_stats.min = sampler.getMin();
    ret_stats.max = sampler.getMax();
    ret_stats.sample_count = sampler.getNumSamples();
    /// NOTE - Reset the buffer.
    sampler.clear();
  }
  return ret_stats;
}

static bool aanderaaSensorWatchdogHandler(void *arg) {
  (void)(arg);
  SensorWatchdog::sensor_watchdog_t *watchdog =
      reinterpret_cast<SensorWatchdog::sensor_watchdog_t *>(arg);
  printf("Disabling Aanderaa power\n");
  if (watchdog->_triggerCount < watchdog->_max_triggers) {
    IOWrite(&BB_PL_BUCK_EN, 1);
    IOWrite(&BB_VBUS_EN, 1);
    /*
     * To ensure the Aanderaa processor resets, we send a No-op command (";") after waiting
     * 100 ms for the 9V rail to bleed off. This wakes up the process, which drains some internal
     * capacitance. Alternatively, set AANDERAA_RESET_TIME_MS to at least 5 minutes.
     * See for more detail: https://www.notion.so/sofarocean/FTL-recovery-works-Manually-stop-aanderaa-to-test-edb988cab3a3467caf2aab9f980fcb11
     */
    vTaskDelay(pdMS_TO_TICKS(AANDERAA_TX_TICKLE_DELAY));
    PLUART::write((uint8_t *)tx_tickle_data, strlen(tx_tickle_data));
    vTaskDelay(pdMS_TO_TICKS(AANDERAA_RESET_TIME_MS));
    printf("Re-enabling Aanderaa power\n");
    IOWrite(&BB_PL_BUCK_EN, 0);
    IOWrite(&BB_VBUS_EN, 0);
  } else {
    IOWrite(&BB_PL_BUCK_EN, 1);
    IOWrite(&BB_VBUS_EN, 1);
  }
  return true;
}

static int createAanderaaDataTopic(void) {
  int topiclen = snprintf(aanderaaTopic, BM_TOPIC_MAX_LEN,
                          "sensor/%016" PRIx64 "/sofar/aanderaa", getNodeId());
  configASSERT(topiclen > 0);
  return topiclen;
}

void setup(void) {
  /* USER ONE-TIME SETUP CODE GOES HERE */
  // Retrieve user-set config values out of NVM.
  get_config_float(BM_CFG_PARTITION_SYSTEM, s_current_agg_period_min,
                   strlen(s_current_agg_period_min), &current_agg_period_min);
  get_config_uint(BM_CFG_PARTITION_SYSTEM, s_n_skip_readings, strlen(s_n_skip_readings),
                  &n_skip_readings);
  get_config_uint(BM_CFG_PARTITION_SYSTEM, s_reading_interval_ms, strlen(s_reading_interval_ms),
                  &reading_interval_ms);
  get_config_uint(BM_CFG_PARTITION_SYSTEM, s_payload_wd_to_s, strlen(s_payload_wd_to_s),
                  &payload_wd_to_s);
  get_config_uint(BM_CFG_PARTITION_SYSTEM, s_pl_uart_baud_rate, strlen(s_pl_uart_baud_rate),
                  &baud_rate);
  get_config_uint(BM_CFG_PARTITION_SYSTEM, s_mfg_tx_test_enable, strlen(s_mfg_tx_test_enable),
                  &mfg_tx_test_enable);
  get_config_uint(BM_CFG_PARTITION_SYSTEM, s_sensor_bm_log_enable,
                  strlen(s_sensor_bm_log_enable), &sensorBmLogEnable);

  max_readings_in_agg =
      (((uint64_t)CURRENT_AGG_PERIOD_MS / reading_interval_ms) + N_SAMPLES_PAD);

  // Protect folks from bricking units in the field.
  // If someone sets the current agg period minutes too high,
  // we can crash when allocating too much memory and get into a boot loop.
  constexpr uint32_t MAX_ALLOWED_READINGS = 512;
  max_readings_in_agg =
      max_readings_in_agg > MAX_ALLOWED_READINGS ? MAX_ALLOWED_READINGS : max_readings_in_agg;

  aanderaaTopicStrLen = createAanderaaDataTopic();
  // Allocate memory for pressure data buffer.
  for (uint8_t i = 0; i < NUM_PARAMS_TO_AGG; i++) {
    current_data[i].initBuffer(max_readings_in_agg);
  }
  stats_print_buffer = static_cast<char *>(
      pvPortMalloc(sizeof(char) * 1500)); // Trial and error, should be plenty of space
  // Initialize our LineParser, which will allocated any needed memory for parsing.
  parser.init();
  // Setup the UART – the on-board serial driver that talks to the RS232 transceiver.
  PLUART::init(USER_TASK_PRIORITY);
  // Baud set per configuration.
  PLUART::setBaud(baud_rate);
  // Set a line termination character per protocol of the sensor.
  PLUART::setTerminationCharacter('\n');
  // Turn on the UART.
  PLUART::enable();
  // Enable the input to the Vout power supply and ensure PL buck is disabled.
  IOWrite(&BB_VBUS_EN, 0);
  IOWrite(&BB_PL_BUCK_EN, 1);
  // ensure Aanderaa is off for sufficient time to fully reset.
  vTaskDelay(pdMS_TO_TICKS(AANDERAA_RESET_TIME_MS));
  // enable Vout, 12V by default.
  IOWrite(&BB_PL_BUCK_EN, 0);

  printf("App configs:\n");
  printf("\tcurrent_agg_period_min: %f\n", current_agg_period_min);
  printf("\tCURRENT_AGG_PERIOD_MS: %f\n", CURRENT_AGG_PERIOD_MS);
  printf("\tn_skip_readings: %u\n", n_skip_readings);
  printf("\treading_interval_ms: %u\n", reading_interval_ms);
  printf("\tmax_readings_in_agg: %llu\n", max_readings_in_agg);
  printf("\tpayload_wd_to_s: %u\n", payload_wd_to_s);
  printf("\tbaud_rate: %u\n", baud_rate);
  printf("\tmfg_tx_test_enable: %u\n", mfg_tx_test_enable);
  printf("\tsensorBmLogEnable: %u\n", sensorBmLogEnable);

  SensorWatchdog::SensorWatchdogAdd(AANDERAA_WATCHDOG_ID, PAYLOAD_WATCHDOG_TIMEOUT_MS,
                                    aanderaaSensorWatchdogHandler,
                                    AANDERAA_WATCHDOG_MAX_TRIGGERS, AANDERAA_RAW_LOG);
}

#ifndef FAKE_AANDERAA
static double getDoubleOrNaN(Value value) {
  if (value.type == TYPE_INVALID) {
    return NAN;
  } else {
    return value.data.double_val;
  }
}
static void mfgTestSendTxWakeup(void) {
  static constexpr char wakeup_data[] = "X\r\n";
  PLUART::write((uint8_t *)wakeup_data, strlen(wakeup_data));
}
#endif

void loop(void) {
  /* USER LOOP CODE GOES HERE */
  /// This aggregates BMDK sensor readings into stats, and sends them along to Spotter
  static uint32_t sensorStatsTimer = uptimeGetMs();
  static uint32_t statsStartTick = uptimeGetMs();
  if (CURRENT_AGG_PERIOD_MS > 0 &&
      (uint32_t)uptimeGetMs() - sensorStatsTimer >= CURRENT_AGG_PERIOD_MS) {
    sensorStatsTimer = uptimeGetMs();
    RTCTimeAndDate_t time_and_date = {};
    rtcGet(&time_and_date);
    statsEndRtc = time_and_date;
    // create additional buffers for convenience, we won't allocate values arrays.
#ifdef AANDERAA_BLUE
    currentData_t current_tx_data[NUM_PARAMS_TO_AGG] = {{}, {}, {}, {}, {}, {},
                                                        {}, {}, {}, {}, {}, {}.{}};
#else
    currentData_t current_tx_data[NUM_PARAMS_TO_AGG] = {{}, {}, {}, {}, {}, {},
                                                        {}, {}, {}, {}, {}, {}};
#endif

    for (uint8_t i = 0; i < NUM_PARAMS_TO_AGG; i++) {
      current_tx_data[i] = aggregateStats(current_data[i]);
    }

    char rtcTimeBuffer[32] = {};
    rtcPrint(rtcTimeBuffer, &statsEndRtc);
    uint32_t statsEndTick = uptimeGetMs();
    int buffer_offset =
        sprintf(stats_print_buffer, "rtc_end: %s, tick_start: %u, tick_end: %u | ",
                rtcTimeBuffer, statsStartTick, statsEndTick);
    for (uint8_t i = 0; i < NUM_PARAMS_TO_AGG; i++) {
      buffer_offset +=
          sprintf(stats_print_buffer + buffer_offset,
                  "key: %s, n: %u, min: %.4f, max: %.4f, mean: %.4f, std: %.4f | ", keys[i],
                  current_tx_data[i].sample_count, current_tx_data[i].min,
                  current_tx_data[i].max, current_tx_data[i].mean, current_tx_data[i].stdev);
    }
    printf("DEBUG - wrote %d chars to buffer.", buffer_offset);
    spotter_log(0, "aanderaa_agg.log", USE_TIMESTAMP, "%s\n", stats_print_buffer);
    spotter_log_console(0, "[aanderaa-agg] | %s", stats_print_buffer);
    printf("[aanderaa-agg] | %s\n", stats_print_buffer);
    // Update variables tracking start time of agg period in ticks and RTC.
    statsStartTick = statsEndTick;

    /// Remote Tx using spotter/transmit-data
    // Setup raw bytes for Remote Tx data buffer.
    //    We don't need the full sizeof(currentData_t), don't need pointer to values.
    static const uint8_t N_STAT_ELEM_BYTES = 18;
    static const uint16_t N_TX_DATA_BYTES = N_STAT_ELEM_BYTES * NUM_PARAMS_TO_AGG;
    uint8_t tx_data[N_TX_DATA_BYTES] = {};
    //    // Iterate through all of our aggregated stats, and copy data to our tx buffer.
    for (uint8_t i = 0; i < NUM_PARAMS_TO_AGG; i++) {
      memcpy(tx_data + N_STAT_ELEM_BYTES * i, reinterpret_cast<uint8_t *>(&current_tx_data[i]),
             N_STAT_ELEM_BYTES);
    }
    //    //
    if (spotter_tx_data(tx_data, N_TX_DATA_BYTES, BmNetworkTypeCellularIriFallback)) {
      printf("%" PRIu64 "t - %s | Sucessfully sent %" PRIu16
             " bytes Spotter transmit data request\n",
             uptimeGetMs(), rtcTimeBuffer, N_TX_DATA_BYTES);
    } else {
      printf("%" PRIu64 "t - %s | Failed to send %" PRIu16
             " bytes Spotter transmit data request\n",
             uptimeGetMs(), rtcTimeBuffer, N_TX_DATA_BYTES);
    }
  }

  /// Blink green LED when data is received from the payload UART.
  static bool led2State = false;
  // If LED_GREEN is off and the ledLinePulse flag is set, turn it on.
  if (!led2State && ledLinePulse > -1) {
    IOWrite(&LED_GREEN, BB_LED_ON);
    led2State = true;
  } // If LED_GREEN has been on for LED_ON_TIME_MS, turn it off.
  else if (led2State && ((u_int32_t)uptimeGetMs() - ledLinePulse >= LED_ON_TIME_MS)) {
    IOWrite(&LED_GREEN, BB_LED_OFF);
    ledLinePulse = -1;
    led2State = false;
  }

  // Blink red LED on a simple timer.
  static u_int32_t ledPulseTimer = uptimeGetMs();
  static u_int32_t ledOnTimer = 0;
  static bool led1State = false;
  // Turn LED_RED on every LED_PERIOD_MS milliseconds.
  if (!led1State && ((u_int32_t)uptimeGetMs() - ledPulseTimer >= LED_PERIOD_MS)) {
    IOWrite(&LED_RED, BB_LED_ON);
    ledOnTimer = uptimeGetMs();
    ledPulseTimer += LED_PERIOD_MS;
    led1State = true;
  } // If LED_RED has been on for LED_ON_TIME_MS milliseconds, turn it off.
  else if (led1State && ((u_int32_t)uptimeGetMs() - ledOnTimer >= LED_ON_TIME_MS)) {
    IOWrite(&LED_RED, BB_LED_OFF);
    led1State = false;
  }

#ifdef FAKE_AANDERAA
  (void)readings_skipped;
  spoof_aanderaa();
#else  // FAKE_AANDERAA

  // Manufacturing TX test mode
  if (mfg_tx_test_enable) {
    static uint32_t mfgTxTimerMs = uptimeGetMs();
    if (uptimeGetMs() - mfgTxTimerMs >= MFG_TEST_TX_PERIOD_MS) {
      mfgTxTimerMs = uptimeGetMs();
      mfgTestSendTxWakeup();
    }
  }

  // If there is a line to read, read it ad then parse it
  if (PLUART::lineAvailable()) {
    SensorWatchdog::SensorWatchdogPet(AANDERAA_WATCHDOG_ID);
    uint16_t read_len = PLUART::readLine(payload_buffer, sizeof(payload_buffer));

    char rtcTimeBuffer[32] = {};
    rtcPrint(rtcTimeBuffer, NULL);
    if (sensorBmLogEnable) {
      spotter_log(0, AANDERAA_RAW_LOG, USE_TIMESTAMP, "tick: %" PRIu64 ", rtc: %s, line: %.*s\n",
                 uptimeGetMs(), rtcTimeBuffer, read_len, payload_buffer);
    }
    spotter_log_console(0, "[aanderaa] | tick: %" PRIu64 ", rtc: %s, line: %.*s", uptimeGetMs(),
              rtcTimeBuffer, read_len, payload_buffer);
    printf("[aanderaa] | tick: %" PRIu64 ", rtc: %s, line: %.*s\n", uptimeGetMs(),
           rtcTimeBuffer, read_len, payload_buffer);

    ledLinePulse = uptimeGetMs(); // trigger a pulse on LED2
    // Now when we get a line of text data, our LineParser turns it into numeric values.
    if (!parser.parseLine(payload_buffer, read_len)) {
      printf("Error parsing line!\n");
      return;
    }
    if (readings_skipped < n_skip_readings) {
      readings_skipped++;
      printf("Skipping reading during init.\n");
      return; // TODO - putting a sneaky early return here is really brittle
    }

    // Publish individual reading.
    static AanderaaDataMsg::Data d;
    size_t bufsize =
        sizeof(payload_buffer); // Re-use the payload buffer since we don't need it anymore.
    size_t encoded_len = 0;
    d.header.version = AanderaaDataMsg::VERSION;
    d.header.reading_uptime_millis = uptimeGetMs();
    d.abs_speed_cm_s = getDoubleOrNaN(parser.getValue(ABS_SPEED));
    d.abs_speed_cm_s = getDoubleOrNaN(parser.getValue(ABS_SPEED));
    d.abs_tilt_deg = getDoubleOrNaN(parser.getValue(ABS_TILT));
    d.direction_deg_m = getDoubleOrNaN(parser.getValue(DIRECTION));
    d.east_cm_s = getDoubleOrNaN(parser.getValue(EAST));
    d.heading_deg_m = getDoubleOrNaN(parser.getValue(HEADING));
    d.north_cm_s = getDoubleOrNaN(parser.getValue(NORTH));
    d.single_ping_std_cm_s = getDoubleOrNaN(parser.getValue(SP_STD));
    d.ping_count = getDoubleOrNaN(parser.getValue(PING_COUNT));
    d.tilt_x_deg = getDoubleOrNaN(parser.getValue(TILT_X));
    d.tilt_y_deg = getDoubleOrNaN(parser.getValue(TILT_Y));
    d.max_tilt_deg = getDoubleOrNaN(parser.getValue(MAX_TILT));
    d.std_tilt_deg = getDoubleOrNaN(parser.getValue(STD_TILT));
    d.temperature_deg_c = getDoubleOrNaN(parser.getValue(TEMP));
    RTCTimeAndDate_t datetime;
    if (rtcGet(&datetime) == pdPASS) {
      d.header.reading_time_utc_ms = (rtcGetMicroSeconds(&datetime) / 1e3);
    }

    if (AanderaaDataMsg::encode(d, reinterpret_cast<uint8_t *>(payload_buffer), bufsize,
                                &encoded_len) == CborNoError) {
      bm_pub_wl(aanderaaTopic, aanderaaTopicStrLen, reinterpret_cast<uint8_t *>(payload_buffer),
                encoded_len, 0, BM_COMMON_PUB_SUB_VERSION);
    } else {
      printf("Failed to encode Aanderaa data message\n");
    }

    // Now let's aggregate those values into statistics
    if (current_data[0].getNumSamples() >= max_readings_in_agg &&
        max_readings_in_agg > N_SAMPLES_PAD) {
      printf("ERR - No more room in current reading buffer, already have %d readings!\n",
             max_readings_in_agg);
      return;
    }

    printf("parsed values:\n");
    for (uint8_t i = 0; i < NUM_PARAMS_TO_AGG; i++) {
      double param_reading = parser.getValue(i).data.double_val;
      current_data[i].addSample(param_reading);
      printf("\t%s | value: %f, count: %u/%llu, min: %f, max: %f\n", keys[i], param_reading,
             current_data[i].getNumSamples(), max_readings_in_agg - N_SAMPLES_PAD,
             current_data[i].getMin(), current_data[i].getMax());
    }
  }
#endif // FAKE_AANDERAA
}

#ifdef FAKE_AANDERAA
static void spoof_aanderaa() {
  static constexpr uint32_t FAKE_STATS_PERIOD_MS = 60 * 1000;
  static uint32_t fake_stats_timer_start = uptimeGetMs();
  if (((uint32_t)uptimeGetMs() - fake_stats_timer_start >= FAKE_STATS_PERIOD_MS)) {
    fake_stats_timer_start = uptimeGetMs();
    SensorWatchdog::SensorWatchdogPet(AANDERAA_WATCHDOG_ID);
    // Publish individual reading.
    static AanderaaDataMsg::Data d;
    size_t bufsize =
        sizeof(payload_buffer); // Re-use the payload buffer since we don't need it anymore.
    size_t encoded_len = 0;
    RTCTimeAndDate_t datetime;
    if (rtcGet(&datetime) == pdPASS) {
      d.header.reading_time_utc_ms = (rtcGetMicroSeconds(&datetime) / 1e3);
      srand(d.header.reading_time_utc_ms & 0xFFFFFFFF);
    }
    d.header.version = AanderaaDataMsg::VERSION;
    d.header.reading_uptime_millis = uptimeGetMs();
    // Fill with random values in the range of 0-data from sensor log.
    d.abs_speed_cm_s = 50 + (rand() % 10);
    d.abs_tilt_deg = 7 + (rand() % 3);
    d.direction_deg_m = 150 + (rand() % 10);
    d.east_cm_s = 19 + (rand() % 20);
    d.heading_deg_m = 153 + (rand() % 20);
    d.north_cm_s = 41 + (rand() % 10);
    d.ping_count = 100 + (rand() % 20);
    d.tilt_x_deg = 2 + (rand() % 20);
    d.tilt_y_deg = 2 + (rand() % 20);
    d.max_tilt_deg = 3 + (rand() % 20);
    d.std_tilt_deg = 5 + (rand() % 30);
    d.temperature_deg_c = 20 + (rand() % 5);

    if (AanderaaDataMsg::encode(d, reinterpret_cast<uint8_t *>(payload_buffer), bufsize,
                                &encoded_len) == CborNoError) {
      bm_pub_wl(aanderaaTopic, aanderaaTopicStrLen, reinterpret_cast<uint8_t *>(payload_buffer),
                encoded_len, 0, BM_COMMON_PUB_SUB_VERSION);
    } else {
      printf("Failed to encode Aanderaa data message\n");
    }

    // Now let's aggregate those values into statistics
    if (current_data[0].getNumSamples() >= max_readings_in_agg) {
      printf("ERR - No more room in current reading buffer, already have %d readings!\n",
             max_readings_in_agg);
      return;
    }

    printf("parsed values:\n");
    current_data[ABS_SPEED].addSample(d.abs_speed_cm_s);
    printf("\t%s | value: %f, count: %u/%llu, min: %f, max: %f\n", keys[ABS_SPEED],
           d.abs_speed_cm_s, current_data[ABS_SPEED].getNumSamples(),
           max_readings_in_agg - N_SAMPLES_PAD, current_data[ABS_SPEED].getMin(),
           current_data[ABS_SPEED].getMax());

    current_data[ABS_TILT].addSample(d.abs_tilt_deg);
    printf("\t%s | value: %f, count: %u/%llu, min: %f, max: %f\n", keys[ABS_TILT],
           d.abs_tilt_deg, current_data[ABS_TILT].getNumSamples(),
           max_readings_in_agg - N_SAMPLES_PAD, current_data[ABS_TILT].getMin(),
           current_data[ABS_TILT].getMax());

    current_data[DIRECTION].addSample(d.direction_deg_m);
    printf("\t%s | value: %f, count: %u/%llu, min: %f, max: %f\n", keys[DIRECTION],
           d.direction_deg_m, current_data[DIRECTION].getNumSamples(),
           max_readings_in_agg - N_SAMPLES_PAD, current_data[DIRECTION].getMin(),
           current_data[DIRECTION].getMax());

    current_data[EAST].addSample(d.east_cm_s);
    printf("\t%s | value: %f, count: %u/%llu, min: %f, max: %f\n", keys[EAST], d.east_cm_s,
           current_data[EAST].getNumSamples(), max_readings_in_agg - N_SAMPLES_PAD,
           current_data[EAST].getMin(), current_data[EAST].getMax());

    current_data[HEADING].addSample(d.heading_deg_m);
    printf("\t%s | value: %f, count: %u/%llu, min: %f, max: %f\n", keys[HEADING],
           d.heading_deg_m, current_data[HEADING].getNumSamples(),
           max_readings_in_agg - N_SAMPLES_PAD, current_data[HEADING].getMin(),
           current_data[HEADING].getMax());

    current_data[NORTH].addSample(d.north_cm_s);
    printf("\t%s | value: %f, count: %u/%llu, min: %f, max: %f\n", keys[NORTH], d.north_cm_s,
           current_data[NORTH].getNumSamples(), max_readings_in_agg - N_SAMPLES_PAD,
           current_data[NORTH].getMin(), current_data[NORTH].getMax());

    current_data[PING_COUNT].addSample(d.ping_count);
    printf("\t%s | value: %f, count: %u/%llu, min: %f, max: %f\n", keys[PING_COUNT],
           d.ping_count, current_data[PING_COUNT].getNumSamples(),
           max_readings_in_agg - N_SAMPLES_PAD, current_data[PING_COUNT].getMin(),
           current_data[PING_COUNT].getMax());

    current_data[TILT_X].addSample(d.tilt_x_deg);
    printf("\t%s | value: %f, count: %u/%llu, min: %f, max: %f\n", keys[TILT_X], d.tilt_x_deg,
           current_data[TILT_X].getNumSamples(), max_readings_in_agg - N_SAMPLES_PAD,
           current_data[TILT_X].getMin(), current_data[TILT_X].getMax());

    current_data[TILT_Y].addSample(d.tilt_y_deg);
    printf("\t%s | value: %f, count: %u/%llu, min: %f, max: %f\n", keys[TILT_Y], d.tilt_y_deg,
           current_data[TILT_Y].getNumSamples(), max_readings_in_agg - N_SAMPLES_PAD,
           current_data[TILT_Y].getMin(), current_data[TILT_Y].getMax());

    current_data[MAX_TILT].addSample(d.max_tilt_deg);
    printf("\t%s | value: %f, count: %u/%llu, min: %f, max: %f\n", keys[MAX_TILT],
           d.max_tilt_deg, current_data[MAX_TILT].getNumSamples(),
           max_readings_in_agg - N_SAMPLES_PAD, current_data[MAX_TILT].getMin(),
           current_data[MAX_TILT].getMax());

    current_data[STD_TILT].addSample(d.std_tilt_deg);
    printf("\t%s | value: %f, count: %u/%llu, min: %f, max: %f\n", keys[STD_TILT],
           d.std_tilt_deg, current_data[STD_TILT].getNumSamples(),
           max_readings_in_agg - N_SAMPLES_PAD, current_data[STD_TILT].getMin(),
           current_data[STD_TILT].getMax());

    current_data[TEMP].addSample(d.temperature_deg_c);
    printf("\t%s | value: %f, count: %u/%llu, min: %f, max: %f\n", keys[TEMP],
           d.temperature_deg_c, current_data[TEMP].getNumSamples(),
           max_readings_in_agg - N_SAMPLES_PAD, current_data[TEMP].getMin(),
           current_data[TEMP].getMax());
  }
}
#endif // FAKE_AANDERAA
