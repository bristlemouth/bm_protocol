#include "gtest/gtest.h"

#include "bridgePowerController.h"
#include "fff.h"
#include "mock_stm32_rtc.h"
#include "stm32_io.h"
extern "C" {
#include "mock_FreeRTOS.h"
}

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

// The fixture for testing class Foo.
class BridgePowerControllerTest : public ::testing::Test {
protected:
  // You can remove any or all of the following functions if its body
  // is empty.

  BridgePowerControllerTest() {
    // You can do set-up work for each test here.
  }

  ~BridgePowerControllerTest() override {
    // You can do clean-up work that doesn't throw exceptions here.
  }

  // If the constructor and destructor are not enough for setting up
  // and cleaning up each test, you can define the following methods:

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
    xEventGroupCreate_fake.return_val =
        (EventGroupDef_t *)malloc(sizeof(StaticEventGroup_t)); // lol
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
  static constexpr uint32_t POWER_CONTROLLER_MIN_DELAY_MS = 1000;
  // Objects declared here can be used by all tests in the test suite for Foo.
  IODriver_t fake_io_driver = {.write = fake_io_write_func,
                               .read = fake_io_read_func,
                               .config = NULL,
                               .registerCallback = NULL};
  IOPinHandle_t FAKE_VBUS_EN = {.driver = &fake_io_driver, .pin = NULL};
};

TEST_F(BridgePowerControllerTest, alignment) {
  // Default alignment is 5 minutes, so return values below should all be divisible by 300
  IODriver_t unusedDriver = {
      .write = NULL, .read = NULL, .config = NULL, .registerCallback = NULL};
  IOPinHandle_t unusedPin = {.driver = &unusedDriver, .pin = NULL};
  BridgePowerController powerController(unusedPin);

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
  const uint32_t kTwentyMinutes = 1200000;
  const uint32_t kNineteenMinutes = 1140000;
  const uint32_t kFiveMinutes = 300000;
  const uint32_t kOneMinute = 60000;
  BridgePowerController powerController(FAKE_VBUS_EN, kTwentyMinutes, kNineteenMinutes,
                                        kFiveMinutes, kOneMinute, true, true);
  powerController._update();
  // Init sequence powers the bus on for two minutes
  EXPECT_EQ(fake_io_read_func_fake.call_count, 1);
  EXPECT_EQ(fake_io_write_func_fake.call_count, 1);
  EXPECT_EQ(fake_io_write_func_fake.arg1_history[0], 1);
  // 2 min + MIN_TASK_SLEEP_MS
  uint32_t curtime = 2 * kOneMinute + 1000;
  EXPECT_EQ(xTaskGetTickCount(), curtime);

  // Bridge Controller is now intitialized, but RTC is not set
  // Bus turns off
  powerController._update();
  EXPECT_EQ(fake_io_read_func_fake.call_count, 3);
  EXPECT_EQ(fake_io_write_func_fake.call_count, 2);
  EXPECT_EQ(fake_io_write_func_fake.arg1_history[1], 0);
  curtime += 1000;
  EXPECT_EQ(xTaskGetTickCount(), curtime);

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
  EXPECT_EQ(fake_io_read_func_fake.call_count, 4);
  curtime += 856000; // 14 minutes 16 seconds, to align with hour 2
  EXPECT_EQ(xTaskGetTickCount(), curtime);

  H = 2;
  M = 0;
  S = 0;
  rtcGetMicroSeconds_fake.return_val = 1713232800000000;
  powerController._update();
  EXPECT_EQ(fake_io_read_func_fake.call_count, 5);
  EXPECT_EQ(fake_io_write_func_fake.call_count, 3);
  // The bus stays on for one minute until the next Subample Off time
  curtime += kOneMinute;
  EXPECT_EQ(xTaskGetTickCount(), curtime);

  // Time for a bus down cycle
  M = 1;
  rtcGetMicroSeconds_fake.return_val = 1713232860000000;
  powerController._update();
  EXPECT_EQ(fake_io_read_func_fake.call_count, 6);
  EXPECT_EQ(fake_io_write_func_fake.call_count, 4);
  EXPECT_EQ(fake_io_write_func_fake.arg1_history[3], 0);

  // Task waits through the Subsampling off period until the next interval starts
  curtime += 4 * kOneMinute;
  EXPECT_EQ(xTaskGetTickCount(), curtime);

  // Bus up for next Subsample
  M = 5;
  rtcGetMicroSeconds_fake.return_val = 1713233100000000;
  powerController._update();
  EXPECT_EQ(fake_io_read_func_fake.call_count, 7);
  EXPECT_EQ(fake_io_write_func_fake.call_count, 5);
  EXPECT_EQ(fake_io_write_func_fake.arg1_history[4], 1);
  curtime += kOneMinute;
  EXPECT_EQ(xTaskGetTickCount(), curtime);

  // Turn off for subsampling
  M = 6;
  rtcGetMicroSeconds_fake.return_val = 1713233160000000;
  powerController._update();
  EXPECT_EQ(fake_io_read_func_fake.call_count, 8);
  EXPECT_EQ(fake_io_write_func_fake.call_count, 6);
  EXPECT_EQ(fake_io_write_func_fake.arg1_history[5], 0);
  curtime += 4 * kOneMinute;
  EXPECT_EQ(xTaskGetTickCount(), curtime);

  // Bus on
  M = 10;
  rtcGetMicroSeconds_fake.return_val = 1713233400000000;
  powerController._update();
  EXPECT_EQ(fake_io_read_func_fake.call_count, 9);
  EXPECT_EQ(fake_io_write_func_fake.call_count, 7);
  EXPECT_EQ(fake_io_write_func_fake.arg1_history[6], 1);
  curtime += kOneMinute;
  EXPECT_EQ(xTaskGetTickCount(), curtime);

  // Bus off
  M = 11;
  rtcGetMicroSeconds_fake.return_val = 1713233460000000;
  powerController._update();
  EXPECT_EQ(fake_io_read_func_fake.call_count, 10);
  EXPECT_EQ(fake_io_write_func_fake.call_count, 8);
  EXPECT_EQ(fake_io_write_func_fake.arg1_history[7], 0);
  curtime += 4 * kOneMinute;
  EXPECT_EQ(xTaskGetTickCount(), curtime);

  // Bus on
  M = 15;
  rtcGetMicroSeconds_fake.return_val = 1713233700000000;
  powerController._update();
  EXPECT_EQ(fake_io_read_func_fake.call_count, 11);
  EXPECT_EQ(fake_io_write_func_fake.call_count, 9);
  EXPECT_EQ(fake_io_write_func_fake.arg1_history[8], 1);
  curtime += kOneMinute;
  EXPECT_EQ(xTaskGetTickCount(), curtime);

  // Bus off, sleep until end of sample duration
  M = 16;
  rtcGetMicroSeconds_fake.return_val = 1713233760000000;
  powerController._update();
  EXPECT_EQ(fake_io_read_func_fake.call_count, 12);
  EXPECT_EQ(fake_io_write_func_fake.call_count, 10);
  EXPECT_EQ(fake_io_write_func_fake.arg1_history[9], 0);
  curtime += 3 * kOneMinute;
  EXPECT_EQ(xTaskGetTickCount(), curtime);

  // Stay off and align next sample
  M = 19;
  rtcGetMicroSeconds_fake.return_val = 1713233940000000;
  powerController._update();
  EXPECT_EQ(fake_io_read_func_fake.call_count, 13);
  EXPECT_EQ(fake_io_write_func_fake.call_count, 10);
  curtime += kOneMinute;
  EXPECT_EQ(xTaskGetTickCount(), curtime);

  // Bus on
  M = 20;
  rtcGetMicroSeconds_fake.return_val = 1713234000000000;
  powerController._update();
  EXPECT_EQ(fake_io_read_func_fake.call_count, 14);
  EXPECT_EQ(fake_io_write_func_fake.call_count, 11);
  EXPECT_EQ(fake_io_write_func_fake.arg1_history[10], 1);
  curtime += kOneMinute;
  EXPECT_EQ(xTaskGetTickCount(), curtime);

  // Bus off
  M = 21;
  rtcGetMicroSeconds_fake.return_val = 1713234060000000;
  powerController._update();
  EXPECT_EQ(fake_io_read_func_fake.call_count, 15);
  EXPECT_EQ(fake_io_write_func_fake.call_count, 12);
  EXPECT_EQ(fake_io_write_func_fake.arg1_history[11], 0);
  curtime += 4 * kOneMinute;
  EXPECT_EQ(xTaskGetTickCount(), curtime);
}

// Subsample duration end lines up with sample duration end at 7 minutes.
// Subsample interval is 3 minutes, which is not 5-minute aligned.
TEST_F(BridgePowerControllerTest, subsampling2) {
  const uint32_t kTenMinutes = 600000;
  const uint32_t kSevenMinutes = 420000;
  const uint32_t kThreeMinutes = 180000;
  const uint32_t kOneMinute = 60000;
  BridgePowerController powerController(FAKE_VBUS_EN, kTenMinutes, kSevenMinutes, kThreeMinutes,
                                        kOneMinute, true, true);
  powerController._update();
  // Init sequence powers the bus on for two minutes
  EXPECT_EQ(fake_io_read_func_fake.call_count, 1);
  EXPECT_EQ(fake_io_write_func_fake.call_count, 1);
  EXPECT_EQ(fake_io_write_func_fake.arg1_history[0], 1);
  // 2 min + MIN_TASK_SLEEP_MS
  uint32_t curtime = 2 * kOneMinute + 1000;
  EXPECT_EQ(xTaskGetTickCount(), curtime);

  // Bridge Controller is now intitialized, but RTC is not set
  // Bus turns off
  powerController._update();
  EXPECT_EQ(fake_io_read_func_fake.call_count, 3);
  EXPECT_EQ(fake_io_write_func_fake.call_count, 2);
  EXPECT_EQ(fake_io_write_func_fake.arg1_history[1], 0);
  curtime += 1000;
  EXPECT_EQ(xTaskGetTickCount(), curtime);

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
  EXPECT_EQ(fake_io_read_func_fake.call_count, 4);
  EXPECT_EQ(fake_io_write_func_fake.call_count, 2);
  curtime += 71000; // 1 minute 11 seconds, to align with hour 2
  EXPECT_EQ(xTaskGetTickCount(), curtime);

  // Bus on for one minute for subsampling
  H = 2;
  M = 0;
  S = 0;
  rtcGetMicroSeconds_fake.return_val = 1713232800000000;
  powerController._update();
  EXPECT_EQ(fake_io_read_func_fake.call_count, 5);
  EXPECT_EQ(fake_io_write_func_fake.call_count, 3);
  EXPECT_EQ(fake_io_write_func_fake.arg1_history[2], 1);
  curtime += kOneMinute;
  EXPECT_EQ(xTaskGetTickCount(), curtime);

  // Bus off
  M = 1;
  rtcGetMicroSeconds_fake.return_val = 1713232860000000;
  powerController._update();
  EXPECT_EQ(fake_io_read_func_fake.call_count, 6);
  EXPECT_EQ(fake_io_write_func_fake.call_count, 4);
  EXPECT_EQ(fake_io_write_func_fake.arg1_history[3], 0);
  curtime += 2 * kOneMinute;
  EXPECT_EQ(xTaskGetTickCount(), curtime);

  // Bus on
  M = 3;
  rtcGetMicroSeconds_fake.return_val = 1713232980000000;
  powerController._update();
  EXPECT_EQ(fake_io_read_func_fake.call_count, 7);
  EXPECT_EQ(fake_io_write_func_fake.call_count, 5);
  EXPECT_EQ(fake_io_write_func_fake.arg1_history[4], 1);
  curtime += kOneMinute;
  EXPECT_EQ(xTaskGetTickCount(), curtime);

  // Bus off
  M = 4;
  rtcGetMicroSeconds_fake.return_val = 1713233040000000;
  powerController._update();
  EXPECT_EQ(fake_io_read_func_fake.call_count, 8);
  EXPECT_EQ(fake_io_write_func_fake.call_count, 6);
  EXPECT_EQ(fake_io_write_func_fake.arg1_history[5], 0);
  curtime += 2 * kOneMinute;
  EXPECT_EQ(xTaskGetTickCount(), curtime);

  // Bus on
  M = 6;
  rtcGetMicroSeconds_fake.return_val = 1713233160000000;
  powerController._update();
  EXPECT_EQ(fake_io_read_func_fake.call_count, 9);
  EXPECT_EQ(fake_io_write_func_fake.call_count, 7);
  EXPECT_EQ(fake_io_write_func_fake.arg1_history[6], 1);
  curtime += kOneMinute;
  EXPECT_EQ(xTaskGetTickCount(), curtime);

  // Bus off and wait for next aligned sample interval
  M = 7;
  rtcGetMicroSeconds_fake.return_val = 1713233220000000;
  powerController._update();
  EXPECT_EQ(fake_io_read_func_fake.call_count, 10);
  EXPECT_EQ(fake_io_write_func_fake.call_count, 8);
  EXPECT_EQ(fake_io_write_func_fake.arg1_history[7], 0);
  curtime += kThreeMinutes;
  EXPECT_EQ(xTaskGetTickCount(), curtime);

  // Bus on
  M = 10;
  rtcGetMicroSeconds_fake.return_val = 1713233400000000;
  powerController._update();
  EXPECT_EQ(fake_io_read_func_fake.call_count, 11);
  EXPECT_EQ(fake_io_write_func_fake.call_count, 9);
  EXPECT_EQ(fake_io_write_func_fake.arg1_history[8], 1);
  curtime += kOneMinute;
  EXPECT_EQ(xTaskGetTickCount(), curtime);

  // Bus off
  M = 11;
  rtcGetMicroSeconds_fake.return_val = 1713233460000000;
  powerController._update();
  EXPECT_EQ(fake_io_read_func_fake.call_count, 12);
  EXPECT_EQ(fake_io_write_func_fake.call_count, 10);
  EXPECT_EQ(fake_io_write_func_fake.arg1_history[9], 0);
  curtime += 2 * kOneMinute;
  EXPECT_EQ(xTaskGetTickCount(), curtime);
}

TEST_F(BridgePowerControllerTest, goldenPath) {
  BridgePowerController BridgePowerController(
      FAKE_VBUS_EN, BridgePowerController::DEFAULT_SAMPLE_INTERVAL_S * 1000,
      SAMPLE_DURATION_S * 1000, BridgePowerController::DEFAULT_SUBSAMPLE_INTERVAL_S * 1000,
      BridgePowerController::DEFAULT_SUBSAMPLE_DURATION_S * 1000);
  BridgePowerController._update();
  // Init sequence powers the bus on for two minutes
  EXPECT_EQ(fake_io_read_func_fake.call_count, 1);
  EXPECT_EQ(fake_io_write_func_fake.call_count, 1);
  EXPECT_EQ(fake_io_write_func_fake.arg1_history[0], 1);
  // 2 min + MIN_TASK_SLEEP_MS
  EXPECT_EQ(xTaskGetTickCount(), (2 * 60 * 1000 + 1000));

  // Bridge Controller is now intitialized, not enabled and RTC is not set
  // Bus should still be on
  BridgePowerController._update();
  EXPECT_EQ(fake_io_read_func_fake.call_count, 2);
  EXPECT_EQ(fake_io_write_func_fake.call_count, 1);

  // RTC gets set.
  y = 2023;
  m = 5;
  d = 4;
  rtcGetMicroSeconds_fake.return_val = 0;
  isRTCSet_fake.return_val = true;
  BridgePowerController._update();
  EXPECT_EQ(isRTCSet_fake.call_count, 3);
  EXPECT_EQ(fake_io_read_func_fake.call_count, 3);

  // Scheduler is still disabled
  BridgePowerController._update();
  EXPECT_EQ(fake_io_read_func_fake.call_count, 4);
  EXPECT_EQ(fake_io_write_func_fake.call_count, 1);

  // Enable the scheduler
  xTaskSetTickCount(0); // Convinience tick set for checking sleep.
  BridgePowerController.powerControlEnable(true);
  BridgePowerController._update();
  EXPECT_EQ(fake_io_read_func_fake.call_count, 5);
  EXPECT_EQ(fake_io_write_func_fake.call_count, 1);

  // The bus stays on until the next Sample Off time
  uint32_t curtime =
      (BridgePowerController::DEFAULT_SAMPLE_INTERVAL_S + SAMPLE_DURATION_S) * 1000;
  EXPECT_EQ(xTaskGetTickCount(), curtime);

  // Time for a bus down cycle
  rtcGetMicroSeconds_fake.return_val = curtime * 1000;
  BridgePowerController._update();
  EXPECT_EQ(fake_io_read_func_fake.call_count, 6);
  EXPECT_EQ(fake_io_write_func_fake.call_count, 2);
  EXPECT_EQ(fake_io_write_func_fake.arg1_history[1], 0);

  // Task waits through the sampling off period until the next interval starts
  curtime += (BridgePowerController::DEFAULT_SAMPLE_INTERVAL_S - SAMPLE_DURATION_S) * 1000;
  EXPECT_EQ(xTaskGetTickCount(), curtime);

  // Enable Subsampling
  BridgePowerController.subsampleEnable(true);

  // bus up
  rtcGetMicroSeconds_fake.return_val = curtime * 1000;
  BridgePowerController._update();
  EXPECT_EQ(fake_io_read_func_fake.call_count, 7);
  EXPECT_EQ(fake_io_write_func_fake.call_count, 3);
  EXPECT_EQ(fake_io_write_func_fake.arg1_history[2], 1);
  curtime += BridgePowerController::DEFAULT_SUBSAMPLE_DURATION_S * 1000;
  EXPECT_EQ(xTaskGetTickCount(), curtime);

  // Turn off for subsampling
  rtcGetMicroSeconds_fake.return_val = curtime * 1000;
  BridgePowerController._update();
  EXPECT_EQ(fake_io_read_func_fake.call_count, 8);
  EXPECT_EQ(fake_io_write_func_fake.call_count, 4);
  EXPECT_EQ(fake_io_write_func_fake.arg1_history[3], 0);
  curtime += (BridgePowerController::DEFAULT_SUBSAMPLE_INTERVAL_S -
              BridgePowerController::DEFAULT_SUBSAMPLE_DURATION_S) *
             1000;
  EXPECT_EQ(xTaskGetTickCount(), curtime);
}

TEST_F(BridgePowerControllerTest, goldenPathUsingTicks) {
  BridgePowerController BridgePowerController(
      FAKE_VBUS_EN, BridgePowerController::DEFAULT_SAMPLE_INTERVAL_S * 1000,
      SAMPLE_DURATION_S * 1000, BridgePowerController::DEFAULT_SUBSAMPLE_INTERVAL_S * 1000,
      BridgePowerController::DEFAULT_SUBSAMPLE_DURATION_S * 1000, false, false,
      BridgePowerController::DEFAULT_ALIGNMENT_S, true);
  BridgePowerController._update();
  // Init sequence powers the bus on for two minutes
  EXPECT_EQ(fake_io_read_func_fake.call_count, 2);
  EXPECT_EQ(fake_io_write_func_fake.call_count, 1);
  EXPECT_EQ(fake_io_write_func_fake.arg1_history[0], 1);
  // Our timebase is already enabled, so we sleep for the entire interval time.
  uint32_t curtime = BridgePowerController::DEFAULT_SAMPLE_INTERVAL_S * 1000;
  EXPECT_EQ(xTaskGetTickCount(), curtime);

  // Bridge Controller is now intitialized, not enabled and Ticks is set
  // Bus should still be on
  BridgePowerController._update();
  EXPECT_EQ(fake_io_read_func_fake.call_count, 3);
  EXPECT_EQ(fake_io_write_func_fake.call_count, 1);
  EXPECT_EQ(fake_io_write_func_fake.arg1_history[0], 1);
  curtime += POWER_CONTROLLER_MIN_DELAY_MS;
  EXPECT_EQ(xTaskGetTickCount(), curtime);

  // Scheduler is still disabled
  BridgePowerController._update();
  EXPECT_EQ(isRTCSet_fake.call_count, 3);
  EXPECT_EQ(fake_io_read_func_fake.call_count, 4);
  EXPECT_EQ(fake_io_write_func_fake.arg1_history[0], 1);
  curtime += POWER_CONTROLLER_MIN_DELAY_MS;
  EXPECT_EQ(xTaskGetTickCount(), curtime);

  // Enable the scheduler
  BridgePowerController.powerControlEnable(true);
  BridgePowerController._update();
  EXPECT_EQ(fake_io_read_func_fake.call_count, 5);
  EXPECT_EQ(fake_io_write_func_fake.call_count, 1);

  // The bus stays on until the next Sample Off time
  // Aligns to the next interval
  EXPECT_EQ(fake_io_write_func_fake.arg1_history[0], 1);
  curtime += ((BridgePowerController::DEFAULT_SAMPLE_INTERVAL_S + SAMPLE_DURATION_S) * 1000) -
             (2 * POWER_CONTROLLER_MIN_DELAY_MS);
  EXPECT_EQ(xTaskGetTickCount(), curtime);

  // Time for a bus down cycle
  BridgePowerController._update();
  EXPECT_EQ(fake_io_read_func_fake.call_count, 6);
  EXPECT_EQ(fake_io_write_func_fake.call_count, 2);
  EXPECT_EQ(fake_io_write_func_fake.arg1_history[1], 0);

  // Task waits through the sampling off period until the next interval starts
  curtime += (BridgePowerController::DEFAULT_SAMPLE_INTERVAL_S - SAMPLE_DURATION_S) * 1000;
  EXPECT_EQ(xTaskGetTickCount(), curtime);

  // Enable Subsampling
  BridgePowerController.subsampleEnable(true);

  // bus up
  BridgePowerController._update();
  EXPECT_EQ(fake_io_read_func_fake.call_count, 7);
  EXPECT_EQ(fake_io_write_func_fake.call_count, 3);
  EXPECT_EQ(fake_io_write_func_fake.arg1_history[2], 1);
  curtime += BridgePowerController::DEFAULT_SUBSAMPLE_DURATION_S * 1000;
  EXPECT_EQ(xTaskGetTickCount(), curtime);

  // Turn off for subsampling
  BridgePowerController._update();
  EXPECT_EQ(fake_io_read_func_fake.call_count, 8);
  EXPECT_EQ(fake_io_write_func_fake.call_count, 4);
  EXPECT_EQ(fake_io_write_func_fake.arg1_history[3], 0);
  curtime += (BridgePowerController::DEFAULT_SUBSAMPLE_INTERVAL_S -
              BridgePowerController::DEFAULT_SUBSAMPLE_DURATION_S) *
             1000;
  EXPECT_EQ(xTaskGetTickCount(), curtime);
}
