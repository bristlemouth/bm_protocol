#include "gtest/gtest.h"

#include "bridgePowerController.h"
#include "fff.h"
#include "stm32_io.h"
#include "mock_stm32_rtc.h"
extern "C" {
#include "mock_FreeRTOS.h"
}

DEFINE_FFF_GLOBALS;

using namespace testing;

static uint32_t y,m,d,H,M,S;

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
FAKE_VALUE_FUNC(bool, fake_io_read_func, const void *, uint8_t*);

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
     xEventGroupCreate_fake.return_val = (EventGroupDef_t*)malloc(sizeof(StaticEventGroup_t)); // lol 
     xTaskCreate_fake.return_val = pdTRUE;
     fake_io_write_func_fake.return_val = true;
     fake_io_read_func_fake.return_val = true;
     isRTCSet_fake.return_val = false;
     rtcGet_fake.return_val = true;
     SET_CUSTOM_FAKE_SEQ(rtcGet,custom_fakes,1);
  }

  void TearDown() override {
     // Code here will be called immediately after each test (right
     // before the destructor).
     free(xEventGroupCreate_fake.return_val);
  }

    static constexpr uint32_t SAMPLE_DURATION_MS = (3 * 60 * 1000);
  // Objects declared here can be used by all tests in the test suite for Foo.
    IODriver_t fake_io_driver = {.read = fake_io_read_func, .write = fake_io_write_func};
    IOPinHandle_t FAKE_VBUS_EN = {.pin = NULL, .driver = &fake_io_driver};
};

TEST_F(BridgePowerControllerTest, goldenPath) {
    BridgePowerController BridgePowerController(FAKE_VBUS_EN,
        BridgePowerController::DEFAULT_SAMPLE_INTERVAL_MS,  
        SAMPLE_DURATION_MS,
        BridgePowerController::DEFAULT_SAMPLE_INTERVAL_MS,
        BridgePowerController::DEFAULT_SUBSAMPLE_DURATION_MS);
    BridgePowerController._update();
    EXPECT_EQ(fake_io_write_func_fake.call_count, 2); // Init sequence powers the bus on for two minutes, then off.
    EXPECT_EQ(fake_io_write_func_fake.arg1_history[0], 1);
    EXPECT_EQ(fake_io_write_func_fake.arg1_history[1], 0);
    EXPECT_EQ(xTaskGetTickCount(), (2 * 60 * 1000 + 1000));

    // Bridge Controller is now intitialized, not enabled and RTC is not set
    // Bus should still be off
    BridgePowerController._update();
    EXPECT_EQ(fake_io_write_func_fake.call_count, 3); // Init sequence powers the bus on for two minutes, then off.
    EXPECT_EQ(fake_io_write_func_fake.arg1_history[0], 1);
    EXPECT_EQ(fake_io_write_func_fake.arg1_history[1], 0);

    // RTC gets set.
    y= 2023; m = 5; d=4;
    isRTCSet_fake.return_val = true;
    BridgePowerController._update();
    EXPECT_EQ(isRTCSet_fake.call_count,2);

    // We now turn on the bridge (because the scheduler is still disabled)
    BridgePowerController._update();
    EXPECT_EQ(fake_io_read_func_fake.call_count, 3); 
    EXPECT_EQ(fake_io_write_func_fake.call_count, 5);
    EXPECT_EQ(fake_io_write_func_fake.arg1_history[2], 1);

    // Enable the scheduler
    y= 2023; m = 5; d=4; H=0; M=0; S=1;
    xTaskSetTickCount(10000); // Convinience tick set for checking sleep.
    BridgePowerController.powerControlEnable(true);
    BridgePowerController._update();
    EXPECT_EQ(fake_io_write_func_fake.call_count, 6);
    EXPECT_EQ(fake_io_write_func_fake.arg1_history[3], 1);
    EXPECT_EQ(xTaskGetTickCount(), ( 10000 + SAMPLE_DURATION_MS)); // We turn on for a Sample duration
    uint32_t curtime = ( 10000 + SAMPLE_DURATION_MS);

    // Time for a bus down cycle 
    y= 2023; m = 5; d=4; H=0; M=3; S=1;
    BridgePowerController._update();
    EXPECT_EQ(fake_io_write_func_fake.call_count, 7);
    EXPECT_EQ(fake_io_write_func_fake.arg1_history[4], 1);
    EXPECT_EQ(xTaskGetTickCount(), ( curtime + BridgePowerController::DEFAULT_SAMPLE_INTERVAL_MS - SAMPLE_DURATION_MS)); // We turn on for a Sample duration

    // bus up 
    y= 2023; m = 5; d=4; H=0; M=5; S=1;
    BridgePowerController._update();
    EXPECT_EQ(fake_io_write_func_fake.call_count, 8);
    EXPECT_EQ(fake_io_write_func_fake.arg1_history[5], 1);

    // Enable Subsampling
    // Turn off for subsampling
    BridgePowerController.subSampleEnable(true);
    y= 2023; m = 5; d=4; H=0; M=5; S=2;
    BridgePowerController._update();
    EXPECT_EQ(fake_io_write_func_fake.call_count, 9);
    EXPECT_EQ(fake_io_write_func_fake.arg1_history[6], 0);
}
