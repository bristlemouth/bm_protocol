#include "gtest/gtest.h"

#include "bridgePowerController.h"
#include "fff.h"
#include "mock_stm32_rtc.h"
#include "stm32_io.h"
extern "C" {
#include "mock_FreeRTOS.h"
}

#include <memory>

DEFINE_FFF_GLOBALS;

using namespace testing;

static uint32_t y, m, d, H, M, S;

BaseType_t rtcGet_custom_fake(RTCTimeAndDate_t *timeAndDate) {
  timeAndDate->year = y;
  timeAndDate->month = m;
  timeAndDate->day = d;
  timeAndDate->hour = H;
  timeAndDate->minute = M;
  timeAndDate->second = S;
  return pdTRUE;
}

BaseType_t (*custom_fakes[])(RTCTimeAndDate_t *timeAndDate) = {
    rtcGet_custom_fake,
};

static uint8_t persistent_fake_pin_val;
bool io_read_custom_fake(const void *_, uint8_t *pinval) {
  (void)_;
  *pinval = persistent_fake_pin_val;
  return true;
}
bool io_write_custom_fake(const void *_, uint8_t pinval) {
  (void)_;
  persistent_fake_pin_val = pinval;
  return true;
}

FAKE_VALUE_FUNC(bool, fake_io_write_func, const void *, uint8_t);
FAKE_VALUE_FUNC(bool, fake_io_read_func, const void *, uint8_t *);

class BridgePowerControllerTest : public ::testing::Test {
protected:
  std::unique_ptr<BridgePowerController> bridge_power_controller;

  BridgePowerControllerTest() {}

  ~BridgePowerControllerTest() override {}

  void SetUp() override {
    // Code here will be called immediately after the constructor (right
    // before each test).
    RESET_FAKE(xEventGroupCreate);
    RESET_FAKE(xTaskCreate);
    RESET_FAKE(fake_io_write_func);
    RESET_FAKE(fake_io_read_func);
    RESET_FAKE(isRTCSet);
    RESET_FAKE(rtcGet);
    RESET_FAKE(rtcGetMicroSeconds);
    num_io_reads = 0;
    num_io_writes = 0;
    xEventGroupCreate_fake.return_val =
        static_cast<EventGroupDef_t *>(malloc(sizeof(StaticEventGroup_t))); // lol
    xTaskCreate_fake.return_val = pdTRUE;
    fake_io_read_func_fake.custom_fake = io_read_custom_fake;
    fake_io_write_func_fake.custom_fake = io_write_custom_fake;
    persistent_fake_pin_val = 0;
    isRTCSet_fake.return_val = false;
    rtcGet_fake.return_val = true;
    rtcGetMicroSeconds_fake.return_val = 0;
    SET_CUSTOM_FAKE_SEQ(rtcGet, custom_fakes, 1);
    xTaskSetTickCount(0);
  }

  void TearDown() override {
    // Code here will be called immediately after each test (right
    // before the destructor).
    free(xEventGroupCreate_fake.return_val);
  }

  static constexpr uint32_t SAMPLE_DURATION_S = (5 * 60);
  static constexpr uint32_t NUM_IO_READS_PER_BUS_EVENT = (1);
  // There are 2 IOWrites per BUS ON/OFF event
  static constexpr uint32_t NUM_IO_WRITES_PER_BUS_EVENT = (2);
  // tracking variables used by tests to track how many IO reads/writes occur
  uint32_t num_io_reads = 0;
  uint32_t num_io_writes = 0;
  // Objects declared here can be used by all tests in the test suite for Foo.
  IODriver_t fake_io_driver = {.write = fake_io_write_func,
                               .read = fake_io_read_func,
                               .config = NULL,
                               .registerCallback = NULL};
  IOPinHandle_t FAKE_VBUS_EN = {.driver = &fake_io_driver, .pin = NULL};
  IOPinHandle_t FAKE_BOOST_EN = {.driver = &fake_io_driver, .pin = NULL};

  uint32_t duration_start_overall_delay(BridgePowerController::Config config,
                                        bool power_controller_enable = true) {
    uint32_t ret = config.sampleDurationMs + BridgePowerController::CAPACITOR_CHARGE_DELAY_MS;

    if (!power_controller_enable) {
      ret = config.sampleDurationMs;
    }

    return ret;
  }

  uint32_t duration_end_overall_delay(BridgePowerController::Config config,
                                      bool subsample = false) {
    // Assumes that sample interval will start on alignment
    uint32_t ret = config.sampleIntervalMs - config.sampleDurationMs;

    if (subsample) {
      ret = config.subsampleIntervalMs - config.subsampleDurationMs;
    }

    return ret;
  }

  uint32_t init_overall_delay(void) {
    return BridgePowerController::INIT_POWER_ON_TIMEOUT_MS +
           BridgePowerController::MIN_TASK_SLEEP_MS +
           BridgePowerController::CAPACITOR_CHARGE_DELAY_MS;
  }

  uint32_t power_up_delay(BridgePowerController::Config config, bool subsample = false) {
    uint32_t ret = config.sampleDurationMs + BridgePowerController::CAPACITOR_CHARGE_DELAY_MS;

    if (subsample) {
      ret = config.subsampleDurationMs + BridgePowerController::CAPACITOR_CHARGE_DELAY_MS;
    }

    return ret;
  }

  void run_update(uint64_t &current_time_ms, uint32_t time_update_ms, uint8_t expected_read = 0,
                  uint8_t expected_write = 0, bool power_state = false) {
    rtcGetMicroSeconds_fake.return_val = current_time_ms * 1000;
    bridge_power_controller->_update();
    if (expected_read) {
      num_io_reads += NUM_IO_READS_PER_BUS_EVENT * expected_read;
    }
    if (expected_write) {
      EXPECT_EQ(fake_io_write_func_fake.arg1_history[num_io_writes], power_state);
      EXPECT_EQ(fake_io_write_func_fake.arg1_history[num_io_writes + 1], power_state);
      num_io_writes += NUM_IO_WRITES_PER_BUS_EVENT * expected_write;
    }
    EXPECT_EQ(fake_io_read_func_fake.call_count, num_io_reads);
    EXPECT_EQ(fake_io_write_func_fake.call_count, num_io_writes);
    current_time_ms += time_update_ms;
    EXPECT_EQ(xTaskGetTickCount(), current_time_ms);
  }
};

TEST_F(BridgePowerControllerTest, alignment) {
  // Default alignment is 5 minutes, so return values below should all be divisible by 300
  IODriver_t unusedDriver = {
      .write = NULL, .read = NULL, .config = NULL, .registerCallback = NULL};
  IOPinHandle_t unusedPin = {.driver = &unusedDriver, .pin = NULL};
  IOPinHandle_t unusedPin2 = {.driver = &unusedDriver, .pin = NULL};
  BridgePowerController::Config config = {
      .BusLoadSwitchEnablePin = unusedPin,
      .BoostEnablePin = unusedPin2,
  };
  BridgePowerController powerController(config);

  // Starting with default interval of 20 minutes
  uint32_t interval = 1200;

  // Normal first call, last unaligned to next aligned
  uint32_t now = 3799, lastStart = 3503;
  uint32_t next = powerController._alignNextInterval(now, lastStart, interval);
  EXPECT_EQ(next, 4800);

  // When time traveling forward, align to the next interval in the future
  now = 3799, lastStart = 222;
  next = powerController._alignNextInterval(now, lastStart, interval);
  EXPECT_EQ(next, 3900);

  // When time traveling forward, it's ok to align to right now
  now = 3900, lastStart = 1500;
  next = powerController._alignNextInterval(now, lastStart, interval);
  EXPECT_EQ(next, 3900);

  // A second later we have to wait for the following interval
  now = 3901, lastStart = 1500;
  next = powerController._alignNextInterval(now, lastStart, interval);
  EXPECT_EQ(next, 5100);

  // Works with other non-default intervals as well, like an hour
  interval = 3600;
  now = 3799, lastStart = 3503;
  next = powerController._alignNextInterval(now, lastStart, interval);
  EXPECT_EQ(next, 7200);

  // Works even when the sampling interval is not divisible by the alignment
  interval = 1020; // 17 minutes
  now = 3799, lastStart = 3503;
  next = powerController._alignNextInterval(now, lastStart, interval);
  EXPECT_EQ(next, 4800);

  interval = 1380; // 23 minutes
  now = 3799, lastStart = 3503;
  next = powerController._alignNextInterval(now, lastStart, interval);
  EXPECT_EQ(next, 5100);
}

// Sample duration ends at 19 minutes, which is after subsample duration ends at 16 minutes,
// and before the next aligned sample interval starts at 20 minutes.
TEST_F(BridgePowerControllerTest, subsampling1) {
  const uint32_t kTwentyMinutesMs = 1200000;
  const uint32_t kNineteenMinutesMs = 1140000;
  const uint32_t kFiveMinutesMs = 300000;
  const uint32_t kOneMinuteMs = 60000;
  BridgePowerController::Config config = {
      .BusLoadSwitchEnablePin = FAKE_VBUS_EN,
      .BoostEnablePin = FAKE_BOOST_EN,
      .sampleIntervalMs = kTwentyMinutesMs,
      .sampleDurationMs = kNineteenMinutesMs,
      .subsampleIntervalMs = kFiveMinutesMs,
      .subsampleDurationMs = kOneMinuteMs,
      .subsamplingEnabled = true,
      .powerControllerEnabled = true,
  };
  BridgePowerController powerController(config);
  powerController._update();
  num_io_reads += NUM_IO_READS_PER_BUS_EVENT;
  num_io_writes += NUM_IO_WRITES_PER_BUS_EVENT;
  // Init sequence powers the bus on for two minutes
  EXPECT_EQ(fake_io_read_func_fake.call_count, num_io_reads);
  EXPECT_EQ(fake_io_write_func_fake.call_count, num_io_writes);
  EXPECT_EQ(fake_io_write_func_fake.arg1_history[0], 1);
  // 2 min + MIN_TASK_SLEEP_MS
  uint32_t curtimeMs = BridgePowerController::INIT_POWER_ON_TIMEOUT_MS +
                       BridgePowerController::MIN_TASK_SLEEP_MS +
                       BridgePowerController::CAPACITOR_CHARGE_DELAY_MS;
  EXPECT_EQ(xTaskGetTickCount(), curtimeMs);

  // Bridge Controller is now intitialized, but RTC is not set
  // Bus turns off
  powerController._update();
  // IORead is called twice in this case
  num_io_reads += NUM_IO_READS_PER_BUS_EVENT;
  num_io_reads += NUM_IO_READS_PER_BUS_EVENT;
  num_io_writes += NUM_IO_WRITES_PER_BUS_EVENT;
  EXPECT_EQ(fake_io_read_func_fake.call_count, num_io_reads);
  EXPECT_EQ(fake_io_write_func_fake.call_count, num_io_writes);
  EXPECT_EQ(fake_io_write_func_fake.arg1_history[2], 0);
  curtimeMs += BridgePowerController::TIMEBASE_NOT_SET_SLEEP_MS;
  EXPECT_EQ(xTaskGetTickCount(), curtimeMs);

  // RTC gets set
  y = 2024;
  m = 4;
  d = 16;
  H = 1;
  M = 45;
  S = 44;
  rtcGetMicroSeconds_fake.return_val = 1713231944000000;
  isRTCSet_fake.return_val = true;
  powerController._update();
  EXPECT_EQ(isRTCSet_fake.call_count, 5);
  num_io_reads += NUM_IO_READS_PER_BUS_EVENT;
  EXPECT_EQ(fake_io_read_func_fake.call_count, num_io_reads);
  curtimeMs += 856000; // 14 minutes 16 seconds, to align with hour 2
  EXPECT_EQ(xTaskGetTickCount(), curtimeMs);

  H = 2;
  M = 0;
  S = 0;
  rtcGetMicroSeconds_fake.return_val = 1713232800000000;
  powerController._update();
  num_io_reads += NUM_IO_READS_PER_BUS_EVENT;
  num_io_writes += NUM_IO_WRITES_PER_BUS_EVENT;
  EXPECT_EQ(fake_io_read_func_fake.call_count, num_io_reads);
  EXPECT_EQ(fake_io_write_func_fake.call_count, num_io_writes);
  // The bus stays on for one minute until the next Subample Off time
  curtimeMs += (kOneMinuteMs + BridgePowerController::CAPACITOR_CHARGE_DELAY_MS);
  EXPECT_EQ(xTaskGetTickCount(), curtimeMs);

  // Time for a bus down cycle
  M = 1;
  rtcGetMicroSeconds_fake.return_val = 1713232860000000;
  powerController._update();
  num_io_reads += NUM_IO_READS_PER_BUS_EVENT;
  num_io_writes += NUM_IO_WRITES_PER_BUS_EVENT;
  EXPECT_EQ(fake_io_read_func_fake.call_count, num_io_reads);
  EXPECT_EQ(fake_io_write_func_fake.call_count, num_io_writes);
  EXPECT_EQ(fake_io_write_func_fake.arg1_history[6], 0);

  // Task waits through the Subsampling off period until the next interval starts
  curtimeMs += 4 * kOneMinuteMs;
  EXPECT_EQ(xTaskGetTickCount(), curtimeMs);

  // Bus up for next Subsample
  M = 5;
  rtcGetMicroSeconds_fake.return_val = 1713233100000000;
  powerController._update();
  num_io_reads += NUM_IO_READS_PER_BUS_EVENT;
  num_io_writes += NUM_IO_WRITES_PER_BUS_EVENT;
  EXPECT_EQ(fake_io_read_func_fake.call_count, num_io_reads);
  EXPECT_EQ(fake_io_write_func_fake.call_count, num_io_writes);
  EXPECT_EQ(fake_io_write_func_fake.arg1_history[8], 1);
  curtimeMs += kOneMinuteMs + BridgePowerController::CAPACITOR_CHARGE_DELAY_MS;
  EXPECT_EQ(xTaskGetTickCount(), curtimeMs);

  // Turn off for subsampling
  M = 6;
  rtcGetMicroSeconds_fake.return_val = 1713233160000000;
  powerController._update();
  num_io_reads += NUM_IO_READS_PER_BUS_EVENT;
  num_io_writes += NUM_IO_WRITES_PER_BUS_EVENT;
  EXPECT_EQ(fake_io_read_func_fake.call_count, num_io_reads);
  EXPECT_EQ(fake_io_write_func_fake.call_count, num_io_writes);
  EXPECT_EQ(fake_io_write_func_fake.arg1_history[10], 0);
  curtimeMs += 4 * kOneMinuteMs;
  EXPECT_EQ(xTaskGetTickCount(), curtimeMs);

  // Bus on
  M = 10;
  rtcGetMicroSeconds_fake.return_val = 1713233400000000;
  powerController._update();
  num_io_reads += NUM_IO_READS_PER_BUS_EVENT;
  num_io_writes += NUM_IO_WRITES_PER_BUS_EVENT;
  EXPECT_EQ(fake_io_read_func_fake.call_count, num_io_reads);
  EXPECT_EQ(fake_io_write_func_fake.call_count, num_io_writes);
  EXPECT_EQ(fake_io_write_func_fake.arg1_history[12], 1);
  curtimeMs += kOneMinuteMs + BridgePowerController::CAPACITOR_CHARGE_DELAY_MS;
  EXPECT_EQ(xTaskGetTickCount(), curtimeMs);

  // Bus off
  M = 11;
  rtcGetMicroSeconds_fake.return_val = 1713233460000000;
  powerController._update();
  num_io_reads += NUM_IO_READS_PER_BUS_EVENT;
  num_io_writes += NUM_IO_WRITES_PER_BUS_EVENT;
  EXPECT_EQ(fake_io_read_func_fake.call_count, num_io_reads);
  EXPECT_EQ(fake_io_write_func_fake.call_count, num_io_writes);
  EXPECT_EQ(fake_io_write_func_fake.arg1_history[14], 0);
  curtimeMs += 4 * kOneMinuteMs;
  EXPECT_EQ(xTaskGetTickCount(), curtimeMs);

  // Bus on
  M = 15;
  rtcGetMicroSeconds_fake.return_val = 1713233700000000;
  powerController._update();
  num_io_reads += NUM_IO_READS_PER_BUS_EVENT;
  num_io_writes += NUM_IO_WRITES_PER_BUS_EVENT;
  EXPECT_EQ(fake_io_read_func_fake.call_count, num_io_reads);
  EXPECT_EQ(fake_io_write_func_fake.call_count, num_io_writes);
  EXPECT_EQ(fake_io_write_func_fake.arg1_history[16], 1);
  curtimeMs += kOneMinuteMs + BridgePowerController::CAPACITOR_CHARGE_DELAY_MS;
  EXPECT_EQ(xTaskGetTickCount(), curtimeMs);

  // Bus off, sleep until end of sample duration
  M = 16;
  rtcGetMicroSeconds_fake.return_val = 1713233760000000;
  powerController._update();
  num_io_reads += NUM_IO_READS_PER_BUS_EVENT;
  num_io_writes += NUM_IO_WRITES_PER_BUS_EVENT;
  EXPECT_EQ(fake_io_read_func_fake.call_count, num_io_reads);
  EXPECT_EQ(fake_io_write_func_fake.call_count, num_io_writes);
  EXPECT_EQ(fake_io_write_func_fake.arg1_history[18], 0);
  curtimeMs += 3 * kOneMinuteMs;
  EXPECT_EQ(xTaskGetTickCount(), curtimeMs);

  // Stay off and align next sample
  M = 19;
  rtcGetMicroSeconds_fake.return_val = 1713233940000000;
  powerController._update();
  num_io_reads += NUM_IO_READS_PER_BUS_EVENT;
  EXPECT_EQ(fake_io_read_func_fake.call_count, num_io_reads);
  EXPECT_EQ(fake_io_write_func_fake.call_count, num_io_writes);
  curtimeMs += kOneMinuteMs;
  EXPECT_EQ(xTaskGetTickCount(), curtimeMs);

  // Bus on
  M = 20;
  rtcGetMicroSeconds_fake.return_val = 1713234000000000;
  powerController._update();
  num_io_reads += NUM_IO_READS_PER_BUS_EVENT;
  num_io_writes += NUM_IO_WRITES_PER_BUS_EVENT;
  EXPECT_EQ(fake_io_read_func_fake.call_count, num_io_reads);
  EXPECT_EQ(fake_io_write_func_fake.call_count, num_io_writes);
  EXPECT_EQ(fake_io_write_func_fake.arg1_history[20], 1);
  curtimeMs += kOneMinuteMs + BridgePowerController::CAPACITOR_CHARGE_DELAY_MS;
  EXPECT_EQ(xTaskGetTickCount(), curtimeMs);

  // Bus off
  M = 21;
  rtcGetMicroSeconds_fake.return_val = 1713234060000000;
  powerController._update();
  num_io_reads += NUM_IO_READS_PER_BUS_EVENT;
  num_io_writes += NUM_IO_WRITES_PER_BUS_EVENT;
  EXPECT_EQ(fake_io_read_func_fake.call_count, num_io_reads);
  EXPECT_EQ(fake_io_write_func_fake.call_count, num_io_writes);
  EXPECT_EQ(fake_io_write_func_fake.arg1_history[22], 0);
  curtimeMs += 4 * kOneMinuteMs;
  EXPECT_EQ(xTaskGetTickCount(), curtimeMs);
}

// Subsample duration end lines up with sample duration end at 7 minutes.
// Subsample interval is 3 minutes, which is not 5-minute aligned.
TEST_F(BridgePowerControllerTest, subsampling2) {
  const uint32_t kTenMinutesMs = 600000;
  const uint32_t kSevenMinutesMs = 420000;
  const uint32_t kThreeMinutesMs = 180000;
  const uint32_t kOneMinuteMs = 60000;
  BridgePowerController::Config config = {
      .BusLoadSwitchEnablePin = FAKE_VBUS_EN,
      .BoostEnablePin = FAKE_BOOST_EN,
      .sampleIntervalMs = kTenMinutesMs,
      .sampleDurationMs = kSevenMinutesMs,
      .subsampleIntervalMs = kThreeMinutesMs,
      .subsampleDurationMs = kOneMinuteMs,
      .subsamplingEnabled = true,
      .powerControllerEnabled = true,
  };
  BridgePowerController powerController(config);
  powerController._update();
  // Init sequence powers the bus on for two minutes
  num_io_reads += NUM_IO_READS_PER_BUS_EVENT;
  num_io_writes += NUM_IO_WRITES_PER_BUS_EVENT;
  EXPECT_EQ(fake_io_read_func_fake.call_count, num_io_reads);
  EXPECT_EQ(fake_io_write_func_fake.call_count, num_io_writes);
  EXPECT_EQ(fake_io_write_func_fake.arg1_history[0], 1);
  // 2 min + MIN_TASK_SLEEP_MS
  uint32_t curtimeMs = BridgePowerController::INIT_POWER_ON_TIMEOUT_MS +
                       BridgePowerController::MIN_TASK_SLEEP_MS +
                       BridgePowerController::CAPACITOR_CHARGE_DELAY_MS;
  EXPECT_EQ(xTaskGetTickCount(), curtimeMs);

  // Bridge Controller is now intitialized, but RTC is not set
  // Bus turns off
  powerController._update();
  // IORead is called twice in this case
  num_io_reads += NUM_IO_READS_PER_BUS_EVENT;
  num_io_reads += NUM_IO_READS_PER_BUS_EVENT;
  num_io_writes += NUM_IO_WRITES_PER_BUS_EVENT;
  EXPECT_EQ(fake_io_read_func_fake.call_count, num_io_reads);
  EXPECT_EQ(fake_io_write_func_fake.call_count, num_io_writes);
  EXPECT_EQ(fake_io_write_func_fake.arg1_history[2], 0);
  curtimeMs += BridgePowerController::TIMEBASE_NOT_SET_SLEEP_MS;
  EXPECT_EQ(xTaskGetTickCount(), curtimeMs);

  // RTC gets set
  y = 2024;
  m = 4;
  d = 16;
  H = 1;
  M = 58;
  S = 49;
  rtcGetMicroSeconds_fake.return_val = 1713232729000000;
  isRTCSet_fake.return_val = true;
  powerController._update();
  EXPECT_EQ(isRTCSet_fake.call_count, 5);
  num_io_reads += NUM_IO_READS_PER_BUS_EVENT;
  EXPECT_EQ(fake_io_read_func_fake.call_count, num_io_reads);
  EXPECT_EQ(fake_io_write_func_fake.call_count, num_io_writes);
  curtimeMs += 71000; // 1 minute 11 seconds, to align with hour 2
  EXPECT_EQ(xTaskGetTickCount(), curtimeMs);

  // Bus on for one minute for subsampling
  H = 2;
  M = 0;
  S = 0;
  rtcGetMicroSeconds_fake.return_val = 1713232800000000;
  powerController._update();
  num_io_reads += NUM_IO_READS_PER_BUS_EVENT;
  num_io_writes += NUM_IO_WRITES_PER_BUS_EVENT;
  EXPECT_EQ(fake_io_read_func_fake.call_count, num_io_reads);
  EXPECT_EQ(fake_io_write_func_fake.call_count, num_io_writes);
  EXPECT_EQ(fake_io_write_func_fake.arg1_history[4], 1);
  curtimeMs += kOneMinuteMs + BridgePowerController::CAPACITOR_CHARGE_DELAY_MS;
  EXPECT_EQ(xTaskGetTickCount(), curtimeMs);

  // Bus off
  M = 1;
  rtcGetMicroSeconds_fake.return_val = 1713232860000000;
  powerController._update();
  num_io_reads += NUM_IO_READS_PER_BUS_EVENT;
  num_io_writes += NUM_IO_WRITES_PER_BUS_EVENT;
  EXPECT_EQ(fake_io_read_func_fake.call_count, num_io_reads);
  EXPECT_EQ(fake_io_write_func_fake.call_count, num_io_writes);
  EXPECT_EQ(fake_io_write_func_fake.arg1_history[6], 0);
  curtimeMs += 2 * kOneMinuteMs;
  EXPECT_EQ(xTaskGetTickCount(), curtimeMs);

  // Bus on
  M = 3;
  rtcGetMicroSeconds_fake.return_val = 1713232980000000;
  powerController._update();
  num_io_reads += NUM_IO_READS_PER_BUS_EVENT;
  num_io_writes += NUM_IO_WRITES_PER_BUS_EVENT;
  EXPECT_EQ(fake_io_read_func_fake.call_count, num_io_reads);
  EXPECT_EQ(fake_io_write_func_fake.call_count, num_io_writes);
  EXPECT_EQ(fake_io_write_func_fake.arg1_history[8], 1);
  curtimeMs += kOneMinuteMs + BridgePowerController::CAPACITOR_CHARGE_DELAY_MS;
  EXPECT_EQ(xTaskGetTickCount(), curtimeMs);

  // Bus off
  M = 4;
  rtcGetMicroSeconds_fake.return_val = 1713233040000000;
  powerController._update();
  num_io_reads += NUM_IO_READS_PER_BUS_EVENT;
  num_io_writes += NUM_IO_WRITES_PER_BUS_EVENT;
  EXPECT_EQ(fake_io_read_func_fake.call_count, num_io_reads);
  EXPECT_EQ(fake_io_write_func_fake.call_count, num_io_writes);
  EXPECT_EQ(fake_io_write_func_fake.arg1_history[10], 0);
  curtimeMs += 2 * kOneMinuteMs;
  EXPECT_EQ(xTaskGetTickCount(), curtimeMs);

  // Bus on
  M = 6;
  rtcGetMicroSeconds_fake.return_val = 1713233160000000;
  powerController._update();
  num_io_reads += NUM_IO_READS_PER_BUS_EVENT;
  num_io_writes += NUM_IO_WRITES_PER_BUS_EVENT;
  EXPECT_EQ(fake_io_read_func_fake.call_count, num_io_reads);
  EXPECT_EQ(fake_io_write_func_fake.call_count, num_io_writes);
  EXPECT_EQ(fake_io_write_func_fake.arg1_history[12], 1);
  curtimeMs += kOneMinuteMs + BridgePowerController::CAPACITOR_CHARGE_DELAY_MS;
  EXPECT_EQ(xTaskGetTickCount(), curtimeMs);

  // Bus off and wait for next aligned sample interval
  M = 7;
  rtcGetMicroSeconds_fake.return_val = 1713233220000000;
  powerController._update();
  num_io_reads += NUM_IO_READS_PER_BUS_EVENT;
  num_io_writes += NUM_IO_WRITES_PER_BUS_EVENT;
  EXPECT_EQ(fake_io_read_func_fake.call_count, num_io_reads);
  EXPECT_EQ(fake_io_write_func_fake.call_count, num_io_writes);
  EXPECT_EQ(fake_io_write_func_fake.arg1_history[14], 0);
  curtimeMs += kThreeMinutesMs;
  EXPECT_EQ(xTaskGetTickCount(), curtimeMs);

  // Bus on
  M = 10;
  rtcGetMicroSeconds_fake.return_val = 1713233400000000;
  powerController._update();
  num_io_reads += NUM_IO_READS_PER_BUS_EVENT;
  num_io_writes += NUM_IO_WRITES_PER_BUS_EVENT;
  EXPECT_EQ(fake_io_read_func_fake.call_count, num_io_reads);
  EXPECT_EQ(fake_io_write_func_fake.call_count, num_io_writes);
  EXPECT_EQ(fake_io_write_func_fake.arg1_history[16], 1);
  curtimeMs += kOneMinuteMs + BridgePowerController::CAPACITOR_CHARGE_DELAY_MS;
  EXPECT_EQ(xTaskGetTickCount(), curtimeMs);

  // Bus off
  M = 11;
  rtcGetMicroSeconds_fake.return_val = 1713233460000000;
  powerController._update();
  num_io_reads += NUM_IO_READS_PER_BUS_EVENT;
  num_io_writes += NUM_IO_WRITES_PER_BUS_EVENT;
  EXPECT_EQ(fake_io_read_func_fake.call_count, num_io_reads);
  EXPECT_EQ(fake_io_write_func_fake.call_count, num_io_writes);
  EXPECT_EQ(fake_io_write_func_fake.arg1_history[18], 0);
  curtimeMs += 2 * kOneMinuteMs;
  EXPECT_EQ(xTaskGetTickCount(), curtimeMs);
}

// Sample duration ends at 19 minutes, which is after subsample duration ends at 16 minutes,
// and before the next aligned sample interval starts at 20 minutes.
// Observed bug to fix: wake earlier than 19 minutes, sample time still remains
TEST_F(BridgePowerControllerTest, subsampling3WakeEarly) {
  const uint32_t kTwentyMinutesMs = 1200000;
  const uint32_t kNineteenMinutesMs = 1140000;
  const uint32_t kFiveMinutesMs = 300000;
  const uint32_t kOneMinuteMs = 60000;
  BridgePowerController::Config config = {
      .BusLoadSwitchEnablePin = FAKE_VBUS_EN,
      .BoostEnablePin = FAKE_BOOST_EN,
      .sampleIntervalMs = kTwentyMinutesMs,
      .sampleDurationMs = kNineteenMinutesMs,
      .subsampleIntervalMs = kFiveMinutesMs,
      .subsampleDurationMs = kOneMinuteMs,
      .subsamplingEnabled = true,
      .powerControllerEnabled = true,
  };
  BridgePowerController powerController(config);
  powerController._update();
  // Init sequence powers the bus on for two minutes
  num_io_reads += NUM_IO_READS_PER_BUS_EVENT;
  num_io_writes += NUM_IO_WRITES_PER_BUS_EVENT;
  EXPECT_EQ(fake_io_read_func_fake.call_count, num_io_reads);
  EXPECT_EQ(fake_io_write_func_fake.call_count, num_io_writes);
  EXPECT_EQ(fake_io_write_func_fake.arg1_history[0], 1);
  // 2 min + MIN_TASK_SLEEP_MS
  uint32_t curtimeMs = BridgePowerController::INIT_POWER_ON_TIMEOUT_MS +
                       BridgePowerController::MIN_TASK_SLEEP_MS +
                       BridgePowerController::CAPACITOR_CHARGE_DELAY_MS;
  EXPECT_EQ(xTaskGetTickCount(), curtimeMs);

  // Bridge Controller is now intitialized, but RTC is not set
  // Bus turns off
  powerController._update();
  // IORead is called twice in this case
  num_io_reads += NUM_IO_READS_PER_BUS_EVENT;
  num_io_reads += NUM_IO_READS_PER_BUS_EVENT;
  num_io_writes += NUM_IO_WRITES_PER_BUS_EVENT;
  EXPECT_EQ(fake_io_read_func_fake.call_count, num_io_reads);
  EXPECT_EQ(fake_io_write_func_fake.call_count, num_io_writes);
  EXPECT_EQ(fake_io_write_func_fake.arg1_history[2], 0);
  curtimeMs += BridgePowerController::TIMEBASE_NOT_SET_SLEEP_MS;
  EXPECT_EQ(xTaskGetTickCount(), curtimeMs);

  // RTC gets set
  y = 2024;
  m = 4;
  d = 16;
  H = 1;
  M = 45;
  S = 44;
  rtcGetMicroSeconds_fake.return_val = 1713231944000000;
  isRTCSet_fake.return_val = true;
  powerController._update();
  EXPECT_EQ(isRTCSet_fake.call_count, 5);
  num_io_reads += NUM_IO_READS_PER_BUS_EVENT;
  EXPECT_EQ(fake_io_read_func_fake.call_count, num_io_reads);
  curtimeMs += 856000; // 14 minutes 16 seconds, to align with hour 2
  EXPECT_EQ(xTaskGetTickCount(), curtimeMs);

  H = 2;
  M = 0;
  S = 0;
  rtcGetMicroSeconds_fake.return_val = 1713232800000000;
  powerController._update();
  num_io_reads += NUM_IO_READS_PER_BUS_EVENT;
  num_io_writes += NUM_IO_WRITES_PER_BUS_EVENT;
  EXPECT_EQ(fake_io_read_func_fake.call_count, num_io_reads);
  EXPECT_EQ(fake_io_write_func_fake.call_count, num_io_writes);
  // The bus stays on for one minute until the next Subample Off time
  curtimeMs += kOneMinuteMs + BridgePowerController::CAPACITOR_CHARGE_DELAY_MS;
  EXPECT_EQ(xTaskGetTickCount(), curtimeMs);

  // Time for a bus down cycle
  M = 1;
  rtcGetMicroSeconds_fake.return_val = 1713232860000000;
  powerController._update();
  num_io_reads += NUM_IO_READS_PER_BUS_EVENT;
  num_io_writes += NUM_IO_WRITES_PER_BUS_EVENT;
  EXPECT_EQ(fake_io_read_func_fake.call_count, num_io_reads);
  EXPECT_EQ(fake_io_write_func_fake.call_count, num_io_writes);
  EXPECT_EQ(fake_io_write_func_fake.arg1_history[6], 0);

  // Task waits through the Subsampling off period until the next interval starts
  curtimeMs += 4 * kOneMinuteMs;
  EXPECT_EQ(xTaskGetTickCount(), curtimeMs);

  // Bus up for next Subsample
  M = 5;
  rtcGetMicroSeconds_fake.return_val = 1713233100000000;
  powerController._update();
  num_io_reads += NUM_IO_READS_PER_BUS_EVENT;
  num_io_writes += NUM_IO_WRITES_PER_BUS_EVENT;
  EXPECT_EQ(fake_io_read_func_fake.call_count, num_io_reads);
  EXPECT_EQ(fake_io_write_func_fake.call_count, num_io_writes);
  EXPECT_EQ(fake_io_write_func_fake.arg1_history[8], 1);
  curtimeMs += kOneMinuteMs + BridgePowerController::CAPACITOR_CHARGE_DELAY_MS;
  EXPECT_EQ(xTaskGetTickCount(), curtimeMs);

  // Turn off for subsampling
  M = 6;
  rtcGetMicroSeconds_fake.return_val = 1713233160000000;
  powerController._update();
  num_io_reads += NUM_IO_READS_PER_BUS_EVENT;
  num_io_writes += NUM_IO_WRITES_PER_BUS_EVENT;
  EXPECT_EQ(fake_io_read_func_fake.call_count, num_io_reads);
  EXPECT_EQ(fake_io_write_func_fake.call_count, num_io_writes);
  EXPECT_EQ(fake_io_write_func_fake.arg1_history[10], 0);
  curtimeMs += 4 * kOneMinuteMs;
  EXPECT_EQ(xTaskGetTickCount(), curtimeMs);

  // Bus on
  M = 10;
  rtcGetMicroSeconds_fake.return_val = 1713233400000000;
  powerController._update();
  num_io_reads += NUM_IO_READS_PER_BUS_EVENT;
  num_io_writes += NUM_IO_WRITES_PER_BUS_EVENT;
  EXPECT_EQ(fake_io_read_func_fake.call_count, num_io_reads);
  EXPECT_EQ(fake_io_write_func_fake.call_count, num_io_writes);
  EXPECT_EQ(fake_io_write_func_fake.arg1_history[12], 1);
  curtimeMs += kOneMinuteMs + BridgePowerController::CAPACITOR_CHARGE_DELAY_MS;
  EXPECT_EQ(xTaskGetTickCount(), curtimeMs);

  // Bus off
  M = 11;
  rtcGetMicroSeconds_fake.return_val = 1713233460000000;
  powerController._update();
  num_io_reads += NUM_IO_READS_PER_BUS_EVENT;
  num_io_writes += NUM_IO_WRITES_PER_BUS_EVENT;
  EXPECT_EQ(fake_io_read_func_fake.call_count, num_io_reads);
  EXPECT_EQ(fake_io_write_func_fake.call_count, num_io_writes);
  EXPECT_EQ(fake_io_write_func_fake.arg1_history[14], 0);
  curtimeMs += 4 * kOneMinuteMs;
  EXPECT_EQ(xTaskGetTickCount(), curtimeMs);

  // Bus on
  M = 15;
  rtcGetMicroSeconds_fake.return_val = 1713233700000000;
  powerController._update();
  num_io_reads += NUM_IO_READS_PER_BUS_EVENT;
  num_io_writes += NUM_IO_WRITES_PER_BUS_EVENT;
  EXPECT_EQ(fake_io_read_func_fake.call_count, num_io_reads);
  EXPECT_EQ(fake_io_write_func_fake.call_count, num_io_writes);
  EXPECT_EQ(fake_io_write_func_fake.arg1_history[16], 1);
  curtimeMs += kOneMinuteMs + BridgePowerController::CAPACITOR_CHARGE_DELAY_MS;
  EXPECT_EQ(xTaskGetTickCount(), curtimeMs);

  // Bus off, sleep until end of sample duration
  M = 16;
  rtcGetMicroSeconds_fake.return_val = 1713233760000000;
  powerController._update();
  num_io_reads += NUM_IO_READS_PER_BUS_EVENT;
  num_io_writes += NUM_IO_WRITES_PER_BUS_EVENT;
  EXPECT_EQ(fake_io_read_func_fake.call_count, num_io_reads);
  EXPECT_EQ(fake_io_write_func_fake.call_count, num_io_writes);
  EXPECT_EQ(fake_io_write_func_fake.arg1_history[18], 0);
  curtimeMs += 3 * kOneMinuteMs;
  EXPECT_EQ(xTaskGetTickCount(), curtimeMs);

  // Replicate the bug: wake early
  M = 18;
  S = 59;
  rtcGetMicroSeconds_fake.return_val = 1713233939000000;
  powerController._update();
  EXPECT_EQ(fake_io_read_func_fake.call_count, num_io_reads);
  EXPECT_EQ(fake_io_write_func_fake.call_count, num_io_writes);
  curtimeMs += 1000;
  EXPECT_EQ(xTaskGetTickCount(), curtimeMs);

  // Stay off and align next sample
  M = 19;
  S = 0;
  rtcGetMicroSeconds_fake.return_val = 1713233940000000;
  powerController._update();
  num_io_reads += NUM_IO_READS_PER_BUS_EVENT;
  EXPECT_EQ(fake_io_read_func_fake.call_count, num_io_reads);
  EXPECT_EQ(fake_io_write_func_fake.call_count, num_io_writes);
  curtimeMs += kOneMinuteMs;
  EXPECT_EQ(xTaskGetTickCount(), curtimeMs);

  // Bus on
  M = 20;
  rtcGetMicroSeconds_fake.return_val = 1713234000000000;
  powerController._update();
  num_io_reads += NUM_IO_READS_PER_BUS_EVENT;
  num_io_writes += NUM_IO_WRITES_PER_BUS_EVENT;
  EXPECT_EQ(fake_io_read_func_fake.call_count, num_io_reads);
  EXPECT_EQ(fake_io_write_func_fake.call_count, num_io_writes);
  EXPECT_EQ(fake_io_write_func_fake.arg1_history[20], 1);
  curtimeMs += kOneMinuteMs + BridgePowerController::CAPACITOR_CHARGE_DELAY_MS;
  EXPECT_EQ(xTaskGetTickCount(), curtimeMs);

  // Bus off
  M = 21;
  rtcGetMicroSeconds_fake.return_val = 1713234060000000;
  powerController._update();
  num_io_reads += NUM_IO_READS_PER_BUS_EVENT;
  num_io_writes += NUM_IO_WRITES_PER_BUS_EVENT;
  EXPECT_EQ(fake_io_read_func_fake.call_count, num_io_reads);
  EXPECT_EQ(fake_io_write_func_fake.call_count, num_io_writes);
  EXPECT_EQ(fake_io_write_func_fake.arg1_history[22], 0);
  curtimeMs += 4 * kOneMinuteMs;
  EXPECT_EQ(xTaskGetTickCount(), curtimeMs);
}

TEST_F(BridgePowerControllerTest, goldenPath) {
  BridgePowerController::Config config = {
      .BusLoadSwitchEnablePin = FAKE_VBUS_EN,
      .BoostEnablePin = FAKE_BOOST_EN,
      .sampleIntervalMs = BridgePowerController::DEFAULT_SAMPLE_INTERVAL_S * 1000,
      .sampleDurationMs = SAMPLE_DURATION_S * 1000,
      .subsampleIntervalMs = BridgePowerController::DEFAULT_SUBSAMPLE_INTERVAL_S * 1000,
      .subsampleDurationMs = BridgePowerController::DEFAULT_SUBSAMPLE_DURATION_S * 1000,
      .powerControllerEnabled = false,
  };
  bridge_power_controller = std::make_unique<BridgePowerController>(config);
  uint64_t curtimeMs = 0;

  run_update(curtimeMs, init_overall_delay(), 1, 1, true);
  EXPECT_EQ(isRTCSet_fake.call_count, 1);

  // Bridge Controller is now intitialized, not enabled and RTC is not set
  // Bus should still be on
  run_update(curtimeMs, BridgePowerController::TIMEBASE_NOT_SET_SLEEP_MS, 1);

  // RTC gets set.
  y = 2023;
  m = 5;
  d = 4;
  isRTCSet_fake.return_val = true;
  run_update(curtimeMs, BridgePowerController::TIMEBASE_NOT_SET_SLEEP_MS, 1);
  EXPECT_EQ(isRTCSet_fake.call_count, 3);

  // Power controller is disabled, but,
  // with RTC set will transfer to sampling stage, waking up the task early
  run_update(curtimeMs, config.sampleIntervalMs - curtimeMs +
                            BridgePowerController::CAPACITOR_CHARGE_DELAY_MS);

  xTaskSetTickCount(0); // Convenience tick set for checking sleep.
  curtimeMs = 0;

  // Controller waits until the next aligned sample start
  run_update(curtimeMs, config.sampleIntervalMs);

  // Ensure that power does not turn off when power controller is off,
  // but aggregated reports can still occur
  for (uint8_t i = 0; i < UINT8_MAX; i++) {
    run_update(curtimeMs, duration_start_overall_delay(config, false), 1);

    run_update(curtimeMs, duration_end_overall_delay(config), 1);
  }

  // Enable the scheduler
  bridge_power_controller->powerControlEnable(true);

  // The bus stays on until the next Sample Off time
  run_update(curtimeMs, config.sampleDurationMs, 1);

  // Time for a bus down cycle
  run_update(curtimeMs, duration_end_overall_delay(config), 1, 1, false);

  // Enable Subsampling
  bridge_power_controller->subsampleEnable(true);

  // bus up
  run_update(curtimeMs, power_up_delay(config, true), 1, 1, true);

  // Turn off for subsampling
  run_update(curtimeMs, duration_end_overall_delay(config, true), 1, 1, false);
}

TEST_F(BridgePowerControllerTest, goldenPathUsingTicks) {
  BridgePowerController::Config config = {
      .BusLoadSwitchEnablePin = FAKE_VBUS_EN,
      .BoostEnablePin = FAKE_BOOST_EN,
      .sampleIntervalMs = BridgePowerController::DEFAULT_SAMPLE_INTERVAL_S * 1000,
      .sampleDurationMs = SAMPLE_DURATION_S * 1000,
      .subsampleIntervalMs = BridgePowerController::DEFAULT_SUBSAMPLE_INTERVAL_S * 1000,
      .subsampleDurationMs = BridgePowerController::DEFAULT_SUBSAMPLE_DURATION_S * 1000,
      .subsamplingEnabled = false,
      .powerControllerEnabled = false,
      .alignmentS = BridgePowerController::DEFAULT_ALIGNMENT_S,
      .ticksSamplingEnabled = true,
  };
  bridge_power_controller = std::make_unique<BridgePowerController>(config);
  uint64_t curtimeMs = 0;

  // Init sequence powers the bus on for two minutes
  // IORead is called twice in this case
  run_update(curtimeMs,
             BridgePowerController::DEFAULT_SAMPLE_INTERVAL_S * 1000 +
                 BridgePowerController::CAPACITOR_CHARGE_DELAY_MS,
             2, 1, true);

  // Bridge Controller is tow intitialized, not enabled and Ticks is set
  // Bus should still be on with sampling enabled
  run_update(curtimeMs, config.sampleDurationMs, 1);

  // Sampling interval is finish, bus will stay on
  run_update(curtimeMs, duration_end_overall_delay(config), 1);
  EXPECT_EQ(isRTCSet_fake.call_count, 1);

  // Enable the power controller, this re-aligns the sampling period
  bridge_power_controller->powerControlEnable(true);
  run_update(curtimeMs, config.sampleDurationMs, 1);

  // Time for a bus down cycle
  run_update(curtimeMs, duration_end_overall_delay(config), 1, 1, false);

  // The bus stays off until the next Sample on time
  run_update(curtimeMs, power_up_delay(config), 1, 1, true);

  // Time for a bus down cycle
  run_update(curtimeMs, duration_end_overall_delay(config), 1, 1, false);

  // Enable Subsampling, this does not re-align the sampling period...
  bridge_power_controller->subsampleEnable(true);

  // bus up
  run_update(curtimeMs, power_up_delay(config, true), 1, 1, true);

  // Turn off for subsampling
  run_update(curtimeMs, duration_end_overall_delay(config, true), 1, 1, false);
}

TEST_F(BridgePowerControllerTest, goldenPathIntervalEqualsDuration) {
  BridgePowerController::Config config = {
      .BusLoadSwitchEnablePin = FAKE_VBUS_EN,
      .BoostEnablePin = FAKE_BOOST_EN,
      .sampleIntervalMs = BridgePowerController::DEFAULT_SAMPLE_INTERVAL_S * 1000,
      .sampleDurationMs = BridgePowerController::DEFAULT_SAMPLE_INTERVAL_S * 1000,
      .subsampleIntervalMs = BridgePowerController::DEFAULT_SUBSAMPLE_INTERVAL_S * 1000,
      .subsampleDurationMs = BridgePowerController::DEFAULT_SUBSAMPLE_DURATION_S * 1000,
      .powerControllerEnabled = true,
      .subsamplingEnabled = false,
  };
  bridge_power_controller = std::make_unique<BridgePowerController>(config);
  uint64_t curtimeMs = 0;

  run_update(curtimeMs, init_overall_delay(), 1, 1, true);
  EXPECT_EQ(isRTCSet_fake.call_count, 1);

  // RTC gets set.
  y = 2024;
  m = 10;
  d = 7;
  isRTCSet_fake.return_val = true;
  run_update(curtimeMs, BridgePowerController::TIMEBASE_NOT_SET_SLEEP_MS, 1);
  EXPECT_EQ(isRTCSet_fake.call_count, 2);

  // Interval starts immedietely no alignment if power controller is off
  run_update(curtimeMs, config.sampleDurationMs - curtimeMs +
                            BridgePowerController::CAPACITOR_CHARGE_DELAY_MS);

  for (uint8_t i = 0; i < UINT8_MAX; i++) {
    // Interval end
    run_update(curtimeMs, config.sampleDurationMs, 1);

    // Interval start
    run_update(curtimeMs, BridgePowerController::MIN_TASK_SLEEP_MS);
  }

  // Interval end
  run_update(curtimeMs, config.sampleDurationMs, 1);
}

TEST_F(BridgePowerControllerTest, goldenPathSubsampleIntervalEqualsDuration) {
  BridgePowerController::Config config = {
      .BusLoadSwitchEnablePin = FAKE_VBUS_EN,
      .BoostEnablePin = FAKE_BOOST_EN,
      .sampleIntervalMs = 300 * 1000,
      .sampleDurationMs = 120 * 1000,
      .subsampleIntervalMs = BridgePowerController::DEFAULT_SUBSAMPLE_INTERVAL_S * 1000,
      .subsampleDurationMs = BridgePowerController::DEFAULT_SUBSAMPLE_INTERVAL_S * 1000,
      .powerControllerEnabled = true,
      .subsamplingEnabled = false,
  };
  bridge_power_controller = std::make_unique<BridgePowerController>(config);
  uint64_t curtimeMs = 0;

  // Init sequence powers the bus on for two minutes
  run_update(curtimeMs, init_overall_delay(), 1, 1, true);
  EXPECT_EQ(isRTCSet_fake.call_count, 1);

  // RTC gets set.
  y = 2024;
  m = 10;
  d = 7;
  isRTCSet_fake.return_val = true;
  run_update(curtimeMs,
             config.alignmentS * 1000 - curtimeMs +
                 BridgePowerController::CAPACITOR_CHARGE_DELAY_MS,
             2, 1, false);
  EXPECT_EQ(isRTCSet_fake.call_count, 3);

  // Interval start, subsampling is inherentely disabled,
  // due to the interval and duration being equal
  run_update(curtimeMs, duration_start_overall_delay(config), 1, 1, true);
  // Duration end
  run_update(curtimeMs, duration_end_overall_delay(config), 1, 1, false);
  // Interval start
  run_update(curtimeMs, duration_start_overall_delay(config), 1, 1, true);
  // Duration end
  run_update(curtimeMs, duration_end_overall_delay(config), 1, 1, false);
}
