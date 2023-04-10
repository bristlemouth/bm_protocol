#include "gtest/gtest.h"

#include "tca9546a.h"
#include "fff.h"

using namespace TCA;

DEFINE_FFF_GLOBALS;

FAKE_VALUE_FUNC(I2CResponse_t,i2cTxRx,I2CInterface_t *, uint8_t, uint8_t *, size_t, uint8_t*, size_t, uint32_t);
FAKE_VALUE_FUNC(I2CResponse_t,i2cProbe,I2CInterface_t *, uint8_t, uint32_t);

// The fixture for teting class Foo.
class TCA9546ATest : public ::testing::Test {
 protected:
  // You can remove any or all of the following functions if its body
  // is empty.

  TCA9546ATest() {
    // You can do set-up work for each test here.
  }

  ~TCA9546ATest() override {
    // You can do clean-up work that doesn't throw exceptions here.
  }

  // If the constructor and destructor are not enough for setting up
  // and cleaning up each test, you can define the following methods:

  void SetUp() override {
    // Code here will be called immediately after the constructor (right
    // before each test).
    RESET_FAKE(i2cTxRx);
    RESET_FAKE(i2cProbe);
    i2cTxRx_fake.return_val = I2C_OK;
    i2cProbe_fake.return_val = I2C_OK;
  }

  void TearDown() override {
    // Code here will be called immediately after each test (right
    // before the destructor).
  }

  // Objects declared here can be used by all tests in the test suite for Foo.
};

TEST_F(TCA9546ATest, BasicConfigTest)
{
  I2CInterface_t i2c;
  Channel_t _channel;
  TCA9546A tca(&i2c, 0x70, NULL);
  tca.init();
  EXPECT_EQ(i2cTxRx_fake.call_count, 1);
  tca.getChannel(&_channel);
  EXPECT_EQ(i2cTxRx_fake.call_count, 2);
  tca.setChannel(CH_3);
  EXPECT_EQ(i2cTxRx_fake.call_count, 3);
}