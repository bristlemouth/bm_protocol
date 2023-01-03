#include "gtest/gtest.h"

#include "util.h"

// The fixture for testing class Foo.
class UTCFromDateTime : public ::testing::Test {
 protected:
  // You can remove any or all of the following functions if its body
  // is empty.

  UTCFromDateTime() {
     // You can do set-up work for each test here.
  }

  ~UTCFromDateTime() override {
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

// Check the "beggining of time"
TEST_F(UTCFromDateTime, StartOfTime)
{
  uint32_t timestmap = utcFromDateTime(1970, 1, 1, 0, 0,0);
  EXPECT_EQ(timestmap, 0);
}

// Check time in a leap year pre feb28
TEST_F(UTCFromDateTime, SometimeIn2020)
{
  uint32_t timestmap = utcFromDateTime(2020, 2, 4, 9, 2,3);
  EXPECT_EQ(timestmap, 1580806923);
}

// Check time in a non leap year after feb
TEST_F(UTCFromDateTime, SometimeIn2021)
{
  uint32_t timestmap = utcFromDateTime(2021, 3, 4, 9, 2,3);
  EXPECT_EQ(timestmap, 1614848523);
}

// Check time in a leap year after feb28
TEST_F(UTCFromDateTime, SometimeIn2022)
{
  uint32_t timestmap = utcFromDateTime(2022, 3, 4, 9, 2,3);
  EXPECT_EQ(timestmap, 1646384523);
}

// Last second before new year
TEST_F(UTCFromDateTime, LastSecondOf2023)
{
  uint32_t timestmap = utcFromDateTime(2023, 12, 31, 23, 59, 59);
  EXPECT_EQ(timestmap, 1704067199);
}

// First second of the year!
TEST_F(UTCFromDateTime, FirstSecondOf2024)
{
  uint32_t timestmap = utcFromDateTime(2024, 1, 1, 0, 0, 0);
  EXPECT_EQ(timestmap, 1704067200);
}

// Another leap year check
TEST_F(UTCFromDateTime, Feb282024)
{
  uint32_t timestmap = utcFromDateTime(2024, 2, 28, 23, 59, 59);
  EXPECT_EQ(timestmap, 1709164799);
}

// Another leap year check
TEST_F(UTCFromDateTime, Feb292024)
{
  uint32_t timestmap = utcFromDateTime(2024, 2, 29, 0, 0, 0);
  EXPECT_EQ(timestmap, 1709164800);
}
