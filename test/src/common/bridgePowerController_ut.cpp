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
    fake_io_write_func_fake.return_val = true;
    fake_io_read_func_fake.return_val = true;
    isRTCSet_fake.return_val = false;
    rtcGet_fake.return_val = true;
    rtcGetMicroSeconds_fake.return_val = 0;
    SET_CUSTOM_FAKE_SEQ(rtcGet, custom_fakes, 1);
  }

  void TearDown() override {
    // Code here will be called immediately after each test (right
    // before the destructor).
    free(xEventGroupCreate_fake.return_val);
  }

  static constexpr uint32_t SAMPLE_DURATION_S = (5 * 60);
  // Objects declared here can be used by all tests in the test suite for Foo.
  IODriver_t fake_io_driver = {.write = fake_io_write_func,
                               .read = fake_io_read_func,
                               .config = NULL,
                               .registerCallback = NULL};
  IOPinHandle_t FAKE_VBUS_EN = {.driver = &fake_io_driver, .pin = NULL};
};

TEST_F(BridgePowerControllerTest, goldenPath) {
  BridgePowerController BridgePowerController(
      FAKE_VBUS_EN, BridgePowerController::DEFAULT_SAMPLE_INTERVAL_S * 1000,
      SAMPLE_DURATION_S * 1000, BridgePowerController::DEFAULT_SAMPLE_INTERVAL_S * 1000,
      BridgePowerController::DEFAULT_SUBSAMPLE_DURATION_S * 1000);
  BridgePowerController._update();
  // Init sequence powers the bus on for two minutes
  EXPECT_EQ(fake_io_write_func_fake.call_count, 1);
  EXPECT_EQ(fake_io_write_func_fake.arg1_history[0], 1);
  // 2 min + MIN_TASK_SLEEP_MS
  EXPECT_EQ(xTaskGetTickCount(), (2 * 60 * 1000 + 1000));

  // Bridge Controller is now intitialized, not enabled and RTC is not set
  // Bus should still be on
  BridgePowerController._update();
  EXPECT_EQ(fake_io_write_func_fake.call_count, 2);
  EXPECT_EQ(fake_io_write_func_fake.arg1_history[1], 1);

  // RTC gets set.
  y = 2023;
  m = 5;
  d = 4;
  rtcGetMicroSeconds_fake.return_val = 0;
  isRTCSet_fake.return_val = true;
  BridgePowerController._update();
  EXPECT_EQ(isRTCSet_fake.call_count, 3);

  // We now turn on the bridge (because the scheduler is still disabled)
  BridgePowerController._update();
  EXPECT_EQ(fake_io_read_func_fake.call_count, 7);
  EXPECT_EQ(fake_io_write_func_fake.call_count, 4);
  EXPECT_EQ(fake_io_write_func_fake.arg1_history[3], 1);

  // Enable the scheduler
  xTaskSetTickCount(0); // Convinience tick set for checking sleep.
  BridgePowerController.powerControlEnable(true);
  BridgePowerController._update();
  EXPECT_EQ(fake_io_write_func_fake.call_count, 5);
  EXPECT_EQ(fake_io_write_func_fake.arg1_history[4], 1);
  EXPECT_EQ(xTaskGetTickCount(),
            ((SAMPLE_DURATION_S * 1000))); // We turn on for a Sample duration
  uint32_t curtime = (SAMPLE_DURATION_S * 1000);

  // Time for a bus down cycle
  rtcGetMicroSeconds_fake.return_val = (curtime * 1000) ;
  BridgePowerController._update();
  EXPECT_EQ(fake_io_write_func_fake.call_count, 5);
  EXPECT_EQ(fake_io_write_func_fake.arg1_history[5], 0);
  curtime += ((BridgePowerController::DEFAULT_SAMPLE_INTERVAL_S - SAMPLE_DURATION_S) * 1000);
  EXPECT_EQ(xTaskGetTickCount(),
            (curtime)); // We turn on for a Sample duration

  // Enable Subsampling
  BridgePowerController.subsampleEnable(true);
  // bus up
  rtcGetMicroSeconds_fake.return_val = (curtime * 1000) ;
  BridgePowerController._update();
  EXPECT_EQ(fake_io_write_func_fake.call_count, 6);
  EXPECT_EQ(fake_io_write_func_fake.arg1_history[6], 0);
  curtime += ((BridgePowerController::DEFAULT_SUBSAMPLE_DURATION_S) * 1000);
  EXPECT_EQ(xTaskGetTickCount(),
            (curtime));

  // Turn off for subsampling
  rtcGetMicroSeconds_fake.return_val = (curtime * 1000) ;
  BridgePowerController._update();
  EXPECT_EQ(fake_io_write_func_fake.call_count, 6);
  EXPECT_EQ(fake_io_write_func_fake.arg1_history[7], 0);
  curtime += ((BridgePowerController::DEFAULT_SAMPLE_INTERVAL_S - BridgePowerController::DEFAULT_SUBSAMPLE_DURATION_S) * 1000);
  EXPECT_EQ(xTaskGetTickCount(),
            (curtime));
}

TEST_F(BridgePowerControllerTest, alignment) {
  // Default alignment is 5 minutes, so return values below should all be divisible by 300
  BridgePowerController powerController(FAKE_VBUS_EN);
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
