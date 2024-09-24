#include "gtest/gtest.h"

#include "app_util.h"

// The fixture for testing class Foo.
class UTILTimeRemainingTest : public ::testing::Test {
 protected:
  // You can remove any or all of the following functions if its body
  // is empty.

  UTILTimeRemainingTest() {
     // You can do set-up work for each test here.
  }

  ~UTILTimeRemainingTest() override {
     // You can do clean-up work that doesn't throw exceptions here.
  }

  // If the constructor and destructor are not enough for setting up
  // and cleaning up each test, you can define the following methods:

  void SetUp() override {
     // Code here will be called immediately after the constructor (right
     // before each test).
  }

  void TearDown() override {
     // Code here will be called immediately after each test (right
     // before the destructor).
  }

  // Objects declared here can be used by all tests in the test suite for Foo.
};


TEST_F(UTILTimeRemainingTest, AllZeroes)
{
  uint32_t rem = timeRemainingGeneric(0, 0, 0);
  EXPECT_EQ(rem, 0);
}

TEST_F(UTILTimeRemainingTest, BasicRem)
{
  uint32_t rem = timeRemainingGeneric(0, 5, 10);
  EXPECT_EQ(rem, 5);
}

TEST_F(UTILTimeRemainingTest, BasicNoRemExact)
{
  uint32_t rem = timeRemainingGeneric(0, 10, 10);
  EXPECT_EQ(rem, 0);
}

TEST_F(UTILTimeRemainingTest, BasicNoRemOver)
{
  uint32_t rem = timeRemainingGeneric(0, 15, 10);
  EXPECT_EQ(rem, 0);
}

//
// Test uint32_t overflow
//
TEST_F(UTILTimeRemainingTest, Big)
{
  uint32_t rem = timeRemainingGeneric(UINT32_MAX - 10, UINT32_MAX - 5, 10);
  EXPECT_EQ(rem, 5);
}

TEST_F(UTILTimeRemainingTest, BigNoRem)
{
  uint32_t rem = timeRemainingGeneric(UINT32_MAX - 10, UINT32_MAX, 10);
  EXPECT_EQ(rem, 0);
}

TEST_F(UTILTimeRemainingTest, BigSomeRemOver)
{
  uint32_t rem = timeRemainingGeneric(UINT32_MAX - 10, UINT32_MAX, 15);
  EXPECT_EQ(rem, 5);
}

TEST_F(UTILTimeRemainingTest, BigSomeRemOver2)
{
  uint32_t rem = timeRemainingGeneric(UINT32_MAX - 10, 3, 15);
  EXPECT_EQ(rem, 1);
}

TEST_F(UTILTimeRemainingTest, BigNoRemOver)
{
  uint32_t rem = timeRemainingGeneric(UINT32_MAX - 10, 3, 10);
  EXPECT_EQ(rem, 0);
}

TEST_F(UTILTimeRemainingTest, BigNoRemOver2)
{
  uint32_t rem = timeRemainingGeneric(UINT32_MAX - 10, 200, 15);
  EXPECT_EQ(rem, 0);
}
