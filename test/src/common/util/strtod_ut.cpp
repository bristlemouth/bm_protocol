#include "gtest/gtest.h"

#include "app_util.h"

// The fixture for testing class Foo.
class UTILStrtodTest : public ::testing::Test {
 protected:
  // You can remove any or all of the following functions if its body
  // is empty.

  UTILStrtodTest() {
     // You can do set-up work for each test here.
  }

  ~UTILStrtodTest() override {
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

TEST_F(UTILStrtodTest, SimpleFloat)
{
  char numStr[] = "1.23";
  double dVal = 0;
  bool bRval = bStrtod(numStr, &dVal);
  EXPECT_TRUE(bRval);
  EXPECT_DOUBLE_EQ(dVal, 1.23);
}

TEST_F(UTILStrtodTest, SimpleInteger)
{
  char numStr[] = "123";
  double dVal = 0;
  bool bRval = bStrtod(numStr, &dVal);
  EXPECT_TRUE(bRval);
  EXPECT_DOUBLE_EQ(dVal, 123.0);
}

TEST_F(UTILStrtodTest, FractionOnly)
{
  char numStr[] = "0.123";
  double dVal = 0;
  bool bRval = bStrtod(numStr, &dVal);
  EXPECT_TRUE(bRval);
  EXPECT_DOUBLE_EQ(dVal, 0.123);
}

TEST_F(UTILStrtodTest, StartWithDecimal)
{
  char numStr[] = ".023";
  double dVal = 0;
  bool bRval = bStrtod(numStr, &dVal);
  EXPECT_TRUE(bRval);
  EXPECT_DOUBLE_EQ(dVal, 0.023);
}

TEST_F(UTILStrtodTest, EndWithDecimal)
{
  char numStr[] = "123.";
  double dVal = 0;
  bool bRval = bStrtod(numStr, &dVal);
  EXPECT_FALSE(bRval);
}

TEST_F(UTILStrtodTest, NegativeFloat)
{
  char numStr[] = "-1.54";
  double dVal = 0;
  bool bRval = bStrtod(numStr, &dVal);
  EXPECT_TRUE(bRval);
  EXPECT_DOUBLE_EQ(dVal, -1.54);
}

TEST_F(UTILStrtodTest, NegativeFloat2)
{
  char numStr[] = "-0.54";
  double dVal = 0;
  bool bRval = bStrtod(numStr, &dVal);
  EXPECT_TRUE(bRval);
  EXPECT_DOUBLE_EQ(dVal, -0.54);
}

TEST_F(UTILStrtodTest, LeadingSpaces)
{
  char numStr[] = "  1.23";
  double dVal = 0;
  bool bRval = bStrtod(numStr, &dVal);
  EXPECT_TRUE(bRval);
  EXPECT_DOUBLE_EQ(dVal, 1.23);
}

TEST_F(UTILStrtodTest, LeadingSpacesNegative1)
{
  char numStr[] = "  -1.23";
  double dVal = 0;
  bool bRval = bStrtod(numStr, &dVal);
  EXPECT_TRUE(bRval);
  EXPECT_DOUBLE_EQ(dVal, -1.23);
}

TEST_F(UTILStrtodTest, LeadingSpacesNegative2)
{
  char numStr[] = "  -0.23";
  double dVal = 0;
  bool bRval = bStrtod(numStr, &dVal);
  EXPECT_TRUE(bRval);
  EXPECT_DOUBLE_EQ(dVal, -0.23);
}

// Trailing spaces not supported
TEST_F(UTILStrtodTest, TrailingSpaces)
{
  char numStr[] = "1.23  ";
  double dVal = 0;
  bool bRval = bStrtod(numStr, &dVal);
  EXPECT_FALSE(bRval);
}

// Scientific notation not supported
TEST_F(UTILStrtodTest, ScientificNotation)
{
  char numStr[] = "1.2e-4";
  double dVal = 0;
  bool bRval = bStrtod(numStr, &dVal);
  EXPECT_FALSE(bRval);
}

// Scientific notation not supported
TEST_F(UTILStrtodTest, TinyNumber)
{
  char numStr[] = "0.000000000000000000000000123";
  double dVal = 0;
  bool bRval = bStrtod(numStr, &dVal);
  EXPECT_TRUE(bRval);
  EXPECT_DOUBLE_EQ(dVal, 0.000000000000000000000000123);
}

TEST_F(UTILStrtodTest, BigNumber)
{
  char numStr[] = "123456789012345.1";
  double dVal = 0;
  bool bRval = bStrtod(numStr, &dVal);
  EXPECT_TRUE(bRval);
  EXPECT_DOUBLE_EQ(dVal, 123456789012345.1);
}

TEST_F(UTILStrtodTest, GPS1)
{
  char numStr[] = "3746.36753";
  double dVal = 0;
  bool bRval = bStrtod(numStr, &dVal);
  EXPECT_TRUE(bRval);
  EXPECT_DOUBLE_EQ(dVal, 3746.36753);
}

TEST_F(UTILStrtodTest, GPS2)
{
  char numStr[] = "12223.82427";
  double dVal = 0;
  bool bRval = bStrtod(numStr, &dVal);
  EXPECT_TRUE(bRval);
  EXPECT_DOUBLE_EQ(dVal, 12223.82427);
}

TEST_F(UTILStrtodTest, GPS3)
{
  char numStr[] = "00833.914843";
  double dVal = 0;
  bool bRval = bStrtod(numStr, &dVal);
  EXPECT_TRUE(bRval);
  EXPECT_DOUBLE_EQ(dVal, 833.914843);
}
