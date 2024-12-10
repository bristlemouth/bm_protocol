#include "gtest/gtest.h"

#include "app_util.h"

#define MICROSECONDS_PER_SECOND (1000000)

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
  uint32_t timestamp = utcFromDateTime(1970, 1, 1, 0, 0,0);
  EXPECT_EQ(timestamp, 0);
  utcDateTime_t datetime;
  uint64_t utc_us = static_cast<uint64_t>(timestamp) * MICROSECONDS_PER_SECOND;
  dateTimeFromUtc(utc_us, &datetime);
  EXPECT_EQ(datetime.year, 1970);
  EXPECT_EQ(datetime.month, 1);
  EXPECT_EQ(datetime.day, 1);
  EXPECT_EQ(datetime.hour, 0);
  EXPECT_EQ(datetime.min, 0);
  EXPECT_EQ(datetime.sec, 0);
  EXPECT_EQ(datetime.usec, 0);
}

// Check time in a leap year pre feb28
TEST_F(UTCFromDateTime, SometimeIn2020)
{
  uint32_t timestamp = utcFromDateTime(2020, 2, 4, 9, 2,3);
  EXPECT_EQ(timestamp, 1580806923);
  utcDateTime_t datetime;
  uint64_t utc_us = static_cast<uint64_t>(timestamp) * MICROSECONDS_PER_SECOND;
  dateTimeFromUtc(utc_us, &datetime);
  EXPECT_EQ(datetime.year, 2020);
  EXPECT_EQ(datetime.month, 2);
  EXPECT_EQ(datetime.day, 4);
  EXPECT_EQ(datetime.hour, 9);
  EXPECT_EQ(datetime.min, 2);
  EXPECT_EQ(datetime.sec, 3);
  EXPECT_EQ(datetime.usec, 0);
}

// Check time in a non leap year after feb
TEST_F(UTCFromDateTime, SometimeIn2021)
{
  uint32_t timestamp = utcFromDateTime(2021, 3, 4, 9, 2,3);
  EXPECT_EQ(timestamp, 1614848523);
  utcDateTime_t datetime;
  uint64_t utc_us = static_cast<uint64_t>(timestamp) * MICROSECONDS_PER_SECOND;
  dateTimeFromUtc(utc_us, &datetime);
  EXPECT_EQ(datetime.year, 2021);
  EXPECT_EQ(datetime.month, 3);
  EXPECT_EQ(datetime.day, 4);
  EXPECT_EQ(datetime.hour, 9);
  EXPECT_EQ(datetime.min, 2);
  EXPECT_EQ(datetime.sec, 3);
  EXPECT_EQ(datetime.usec, 0);
}

// Check time in a leap year after feb28
TEST_F(UTCFromDateTime, SometimeIn2022)
{
  uint32_t timestamp = utcFromDateTime(2022, 3, 4, 9, 2,3);
  EXPECT_EQ(timestamp, 1646384523);
  utcDateTime_t datetime;
  uint64_t utc_us = static_cast<uint64_t>(timestamp) * MICROSECONDS_PER_SECOND;
  dateTimeFromUtc(utc_us, &datetime);
  EXPECT_EQ(datetime.year, 2022);
  EXPECT_EQ(datetime.month, 3);
  EXPECT_EQ(datetime.day, 4);
  EXPECT_EQ(datetime.hour, 9);
  EXPECT_EQ(datetime.min, 2);
  EXPECT_EQ(datetime.sec, 3);
  EXPECT_EQ(datetime.usec, 0);
}

// Last second before new year
TEST_F(UTCFromDateTime, LastSecondOf2023)
{
  uint32_t timestamp = utcFromDateTime(2023, 12, 31, 23, 59, 59);
  EXPECT_EQ(timestamp, 1704067199);
  utcDateTime_t datetime;
  uint64_t utc_us = static_cast<uint64_t>(timestamp) * MICROSECONDS_PER_SECOND;
  dateTimeFromUtc(utc_us, &datetime);
  EXPECT_EQ(datetime.year, 2023);
  EXPECT_EQ(datetime.month, 12);
  EXPECT_EQ(datetime.day, 31);
  EXPECT_EQ(datetime.hour, 23);
  EXPECT_EQ(datetime.min, 59);
  EXPECT_EQ(datetime.sec, 59);
  EXPECT_EQ(datetime.usec, 0);
}

// First second of the year!
TEST_F(UTCFromDateTime, FirstSecondOf2024)
{
  uint32_t timestamp = utcFromDateTime(2024, 1, 1, 0, 0, 0);
  EXPECT_EQ(timestamp, 1704067200);
  utcDateTime_t datetime;
  uint64_t utc_us = static_cast<uint64_t>(timestamp) * MICROSECONDS_PER_SECOND;
  dateTimeFromUtc(utc_us, &datetime);
  EXPECT_EQ(datetime.year, 2024);
  EXPECT_EQ(datetime.month, 1);
  EXPECT_EQ(datetime.day, 1);
  EXPECT_EQ(datetime.hour, 0);
  EXPECT_EQ(datetime.min, 0);
  EXPECT_EQ(datetime.sec, 0);
  EXPECT_EQ(datetime.usec, 0);
}

// Another leap year check
TEST_F(UTCFromDateTime, Feb282024)
{
  uint32_t timestamp = utcFromDateTime(2024, 2, 28, 23, 59, 59);
  EXPECT_EQ(timestamp, 1709164799);
  utcDateTime_t datetime;
  uint64_t utc_us = static_cast<uint64_t>(timestamp) * MICROSECONDS_PER_SECOND;
  dateTimeFromUtc(utc_us, &datetime);
  EXPECT_EQ(datetime.year, 2024);
  EXPECT_EQ(datetime.month, 2);
  EXPECT_EQ(datetime.day, 28);
  EXPECT_EQ(datetime.hour, 23);
  EXPECT_EQ(datetime.min, 59);
  EXPECT_EQ(datetime.sec, 59);
  EXPECT_EQ(datetime.usec, 0);
}

// Another leap year check
TEST_F(UTCFromDateTime, Feb292024)
{
  uint32_t timestamp = utcFromDateTime(2024, 2, 29, 0, 0, 0);
  EXPECT_EQ(timestamp, 1709164800);
  utcDateTime_t datetime;
  uint64_t utc_us = static_cast<uint64_t>(timestamp) * MICROSECONDS_PER_SECOND;
  dateTimeFromUtc(utc_us, &datetime);
  EXPECT_EQ(datetime.year, 2024);
  EXPECT_EQ(datetime.month, 2);
  EXPECT_EQ(datetime.day, 29);
  EXPECT_EQ(datetime.hour, 0);
  EXPECT_EQ(datetime.min, 0);
  EXPECT_EQ(datetime.sec, 0);
  EXPECT_EQ(datetime.usec, 0);
}

TEST_F(UTCFromDateTime, MicrosecondsMaxTest)
{
  uint32_t timestamp = utcFromDateTime(2024, 2, 29, 0, 0, 0);
  EXPECT_EQ(timestamp, 1709164800);
  utcDateTime_t datetime;
  uint64_t utc_us = static_cast<uint64_t>(timestamp) * MICROSECONDS_PER_SECOND + (MICROSECONDS_PER_SECOND-1);
  dateTimeFromUtc(utc_us, &datetime);
  EXPECT_EQ(datetime.year, 2024);
  EXPECT_EQ(datetime.month, 2);
  EXPECT_EQ(datetime.day, 29);
  EXPECT_EQ(datetime.hour, 0);
  EXPECT_EQ(datetime.min, 0);
  EXPECT_EQ(datetime.sec, 0);
  EXPECT_EQ(datetime.usec, 999999);
}

TEST_F(UTCFromDateTime, MillisTest)
{
  uint32_t timestamp = utcFromDateTime(2024, 2, 29, 0, 0, 0);
  EXPECT_EQ(timestamp, 1709164800);
  utcDateTime_t datetime;
  uint64_t utc_us = static_cast<uint64_t>(timestamp) * MICROSECONDS_PER_SECOND + (11000);
  dateTimeFromUtc(utc_us, &datetime);
  EXPECT_EQ(datetime.year, 2024);
  EXPECT_EQ(datetime.month, 2);
  EXPECT_EQ(datetime.day, 29);
  EXPECT_EQ(datetime.hour, 0);
  EXPECT_EQ(datetime.min, 0);
  EXPECT_EQ(datetime.sec, 0);
  EXPECT_EQ(datetime.usec, 11000);
}

TEST_F(UTCFromDateTime, Check2038)
{
  uint32_t timestamp = utcFromDateTime(2038, 1, 19, 0, 0, 0);
  EXPECT_EQ(timestamp, 2147472000);
  utcDateTime_t datetime;
  uint64_t utc_us = static_cast<uint64_t>(timestamp) * MICROSECONDS_PER_SECOND;
  dateTimeFromUtc(utc_us, &datetime);
  EXPECT_EQ(datetime.year, 2038);
  EXPECT_EQ(datetime.month, 1);
  EXPECT_EQ(datetime.day, 19);
  EXPECT_EQ(datetime.hour, 0);
  EXPECT_EQ(datetime.min, 0);
  EXPECT_EQ(datetime.sec, 0);
  EXPECT_EQ(datetime.usec, 0);
}
