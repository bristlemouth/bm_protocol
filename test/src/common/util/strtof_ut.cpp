#include "gtest/gtest.h"

#include "app_util.h"

// The fixture for testing class Foo.
class UTILStrtofTest : public ::testing::Test {
 protected:
  // You can remove any or all of the following functions if its body
  // is empty.

  UTILStrtofTest() {
     // You can do set-up work for each test here.
  }

  ~UTILStrtofTest() override {
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


TEST_F(UTILStrtofTest, SimpleFloat)
{
  char numStr[] = "1.23";
  float fVal = 0;
  bool bRval = bStrtof(numStr, &fVal);
  EXPECT_TRUE(bRval);
  EXPECT_FLOAT_EQ(fVal, 1.23);
}

TEST_F(UTILStrtofTest, SimpleInteger)
{
  char numStr[] = "123";
  float fVal = 0;
  bool bRval = bStrtof(numStr, &fVal);
  EXPECT_TRUE(bRval);
  EXPECT_FLOAT_EQ(fVal, 123.0);
}

TEST_F(UTILStrtofTest, FractionOnly)
{
  char numStr[] = "0.123";
  float fVal = 0;
  bool bRval = bStrtof(numStr, &fVal);
  EXPECT_TRUE(bRval);
  EXPECT_FLOAT_EQ(fVal, 0.123);
}

TEST_F(UTILStrtofTest, StartWithDecimal)
{
  char numStr[] = ".023";
  float fVal = 0;
  bool bRval = bStrtof(numStr, &fVal);
  EXPECT_TRUE(bRval);
  EXPECT_FLOAT_EQ(fVal, 0.023);
}

TEST_F(UTILStrtofTest, EndWithDecimal)
{
  char numStr[] = "123.";
  float fVal = 0;
  bool bRval = bStrtof(numStr, &fVal);
  EXPECT_FALSE(bRval);
}

TEST_F(UTILStrtofTest, NegativeFloat)
{
  char numStr[] = "-1.54";
  float fVal = 0;
  bool bRval = bStrtof(numStr, &fVal);
  EXPECT_TRUE(bRval);
  EXPECT_FLOAT_EQ(fVal, -1.54);
}

TEST_F(UTILStrtofTest, NegativeFloat2)
{
  char numStr[] = "-0.54";
  float fVal = 0;
  bool bRval = bStrtof(numStr, &fVal);
  EXPECT_TRUE(bRval);
  EXPECT_FLOAT_EQ(fVal, -0.54);
}

TEST_F(UTILStrtofTest, LeadingSpaces)
{
  char numStr[] = "  1.23";
  float fVal = 0;
  bool bRval = bStrtof(numStr, &fVal);
  EXPECT_TRUE(bRval);
  EXPECT_FLOAT_EQ(fVal, 1.23);
}

TEST_F(UTILStrtofTest, LeadingSpacesNegative1)
{
  char numStr[] = "  -1.23";
  float fVal = 0;
  bool bRval = bStrtof(numStr, &fVal);
  EXPECT_TRUE(bRval);
  EXPECT_FLOAT_EQ(fVal, -1.23);
}

TEST_F(UTILStrtofTest, LeadingSpacesNegative2)
{
  char numStr[] = "  -0.23";
  float fVal = 0;
  bool bRval = bStrtof(numStr, &fVal);
  EXPECT_TRUE(bRval);
  EXPECT_FLOAT_EQ(fVal, -0.23);
}

// Trailing spaces not supported
TEST_F(UTILStrtofTest, TrailingSpaces)
{
  char numStr[] = "1.23  ";
  float fVal = 0;
  bool bRval = bStrtof(numStr, &fVal);
  EXPECT_FALSE(bRval);
}

// Scientific notation not supported
TEST_F(UTILStrtofTest, ScientificNotation)
{
  char numStr[] = "1.2e-4";
  float fVal = 0;
  bool bRval = bStrtof(numStr, &fVal);
  EXPECT_FALSE(bRval);
}

// Scientific notation not supported
TEST_F(UTILStrtofTest, TinyNumber)
{
  char numStr[] = "0.000000000000000000000000123";
  float fVal = 0;
  bool bRval = bStrtof(numStr, &fVal);
  EXPECT_TRUE(bRval);
  EXPECT_FLOAT_EQ(fVal, 0.000000000000000000000000123);
}

TEST_F(UTILStrtofTest, BigNumber)
{
  char numStr[] = "123456789012345.1";
  float fVal = 0;
  bool bRval = bStrtof(numStr, &fVal);
  EXPECT_TRUE(bRval);
  EXPECT_FLOAT_EQ(fVal, 123456789012345.1);
}

// NOTE: There will be some data loss here, but that's the fault of the float
// data type, not the conversion
TEST_F(UTILStrtofTest, GPS1)
{
  char numStr[] = "3746.36753";
  float fVal = 0;
  bool bRval = bStrtof(numStr, &fVal);
  EXPECT_TRUE(bRval);
  EXPECT_FLOAT_EQ(fVal, 3746.36753);
}

// NOTE: There will be some data loss here, but that's the fault of the float
// data type, not the conversion
TEST_F(UTILStrtofTest, GPS2)
{
  char numStr[] = "12223.82427";
  float fVal = 0;
  bool bRval = bStrtof(numStr, &fVal);
  EXPECT_TRUE(bRval);
  EXPECT_FLOAT_EQ(fVal, 12223.82427);
}

// Validate that every number from -1 to 1 (in increments of 0.001)
// can be correctly represented/stored by float
TEST_F(UTILStrtofTest, THREESIGFIG)
{
  char numStr[16];
  float fVal;
  bool bRval;
  double dVal;
  int32_t iVal;
  for(int32_t val = -1000; val < 1000; val++) {
    dVal = (double)val/1000.0;
    snprintf(numStr, sizeof(numStr), "%f", dVal);
    bRval = bStrtof(numStr, &fVal);
    EXPECT_TRUE(bRval);
    iVal = F2INT(fVal*1000);
    EXPECT_DOUBLE_EQ(iVal, val);
  }
}
