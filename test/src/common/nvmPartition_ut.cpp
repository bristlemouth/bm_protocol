#include "gtest/gtest.h"

#include "nvmPartition.h"
#include "mock_storage_driver.h"

using namespace testing;

// The fixture for testing class Foo.
class NvmPartitionTest : public ::testing::Test {
 protected:
  // You can remove any or all of the following functions if its body
  // is empty.

  NvmPartitionTest() {
     // You can do set-up work for each test here.
  }

  ~NvmPartitionTest() override {
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

TEST_F(NvmPartitionTest, BasicTest)
{
  const ext_flash_partition_t test_configuration = {
      .fa_off = 4096,
      .fa_size = 2000,
  };
  MockStorageDriver _storage;
  EXPECT_CALL(_storage, getAlignmentBytes())
    .Times(2)
    .WillRepeatedly(Return(4096));
  EXPECT_CALL(_storage, getStorageSizeBytes())
    .Times(1)
    .WillRepeatedly(Return(8000000));
  NvmPartition testPartition(_storage, test_configuration);
  EXPECT_CALL(_storage, write)
    .Times(1)
    .WillRepeatedly(Return(true));
  uint8_t testbuf[10]; 
  testPartition.write(1000, testbuf, sizeof(testbuf),100);
  EXPECT_CALL(_storage, read)
    .Times(1)
    .WillRepeatedly(Return(true));
  testPartition.read(1000, testbuf, sizeof(testbuf),100);
  EXPECT_CALL(_storage, crc16)
    .Times(1)
    .WillRepeatedly(Return(true));
  uint16_t crc;
  testPartition.crc16(1000, 20, crc,100);
}

TEST_F(NvmPartitionTest, BadInit)
{
  const ext_flash_partition_t bad_test_configuration_a = {
      .fa_off = 201,
      .fa_size = 2000,
  };
  const ext_flash_partition_t bad_test_configuration_b = {
      .fa_off = 201,
      .fa_size = 2000,
  };
  MockStorageDriver _storage;
  EXPECT_CALL(_storage, getAlignmentBytes())
    .Times(AtLeast(0))
    .WillRepeatedly(Return(4096));
  EXPECT_CALL(_storage, getStorageSizeBytes())
    .Times(AtLeast(0))
    .WillRepeatedly(Return(8000000));
  EXPECT_DEATH(NvmPartition testPartition(_storage, bad_test_configuration_a),"");
  EXPECT_DEATH(NvmPartition testPartition(_storage, bad_test_configuration_b),"");
}