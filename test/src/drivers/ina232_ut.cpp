#include "gtest/gtest.h"

#include "ina232.h"
#include "fff.h"

using namespace INA;

DEFINE_FFF_GLOBALS;

FAKE_VALUE_FUNC(I2CResponse_t,i2cTxRx,I2CInterface_t *, uint8_t,uint8_t *, size_t, uint8_t *, size_t, uint32_t);
FAKE_VALUE_FUNC(I2CResponse_t,i2cProbe,I2CInterface_t *, uint8_t, uint32_t);

// The fixture for testing class Foo.
class Ina232Test : public ::testing::Test {
 protected:
  // You can remove any or all of the following functions if its body
  // is empty.

  Ina232Test() {
     // You can do set-up work for each test here.
  }

  ~Ina232Test() override {
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

TEST_F(Ina232Test, BasicConfigTest)
{
  I2CInterface_t i2c;
  INA232 ina(&i2c);
  ina.init();
  EXPECT_EQ(i2cTxRx_fake.call_count, 6);
  ina.setShuntValue(20.0);
  ina.setAvg(AVG_64);
  EXPECT_EQ(i2cTxRx_fake.call_count, 9);
  ina.setBusConvTime(CT_4156);
  EXPECT_EQ(i2cTxRx_fake.call_count, 12);
  ina.setShuntConvTime(CT_8244);
  EXPECT_EQ(i2cTxRx_fake.call_count, 15);
  EXPECT_EQ(ina.getTotalConversionTimeMs(), 793);
}
