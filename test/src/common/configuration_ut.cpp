#include "gtest/gtest.h"

#include "nvmPartition.h"
#include "mock_storage_driver.h"
#include "ram_partitions.h"
#include "configuration.h"
#include "fff.h"

DEFINE_FFF_GLOBALS;

using namespace testing;
using namespace cfg;

// The fixture for testing class Foo.
class ConfigurationTest : public ::testing::Test {
 protected:
  // You can remove any or all of the following functions if its body
  // is empty.

  ConfigurationTest() {
     // You can do set-up work for each test here.
  }

  ~ConfigurationTest() override {
     // You can do clean-up work that doesn't throw exceptions here.
  }

  // If the constructor and destructor are not enough for setting up
  // and cleaning up each test, you can define the following methods:

  void SetUp() override {
     // Code here will be called immediately after the constructor (right
     // before each test).

    // We can ignore these calls, as they are covered in nvmPartition_ut.cpp
    EXPECT_CALL(_storage, getAlignmentBytes())
        .Times(AtLeast(0))
        .WillRepeatedly(Return(4096));
    EXPECT_CALL(_storage, getStorageSizeBytes())
        .Times(AtLeast(0))
        .WillRepeatedly(Return(8000000));
    EXPECT_CALL(_storage, write)
        .Times(AtLeast(0))
        .WillRepeatedly(Return(true));
    EXPECT_CALL(_storage, read)
        .Times(AtLeast(0))
        .WillRepeatedly(Return(true));

    memset(ram_hardware_configuration, 0, RAM_HARDWARE_CONFIG_SIZE_BYTES);
    }

  void TearDown() override {
     // Code here will be called immediately after each test (right
     // before the destructor).
  }

  // Objects declared here can be used by all tests in the test suite for Foo.
  MockStorageDriver _storage;
};

TEST_F(ConfigurationTest, BasicTest)
{
    const ext_flash_partition_t test_configuration = {
        .fa_off = 4096,
        .fa_size = 10000,
    };
    NvmPartition testPartition(_storage, test_configuration);
    Configuration config(testPartition,ram_hardware_configuration,RAM_HARDWARE_CONFIG_SIZE_BYTES);
    uint8_t num_keys;
    const ConfigKey_t *key_list = NULL;
    key_list = config.getStoredKeys(num_keys);
    EXPECT_EQ(num_keys,0);
    EXPECT_EQ((key_list != NULL), true);

//   uint32_t
    uint32_t foo = 42;
    uint32_t result_foo = 0;
    EXPECT_EQ(config.setConfig("foo", strlen("foo"), foo),true);
    EXPECT_EQ(config.getConfig("foo", strlen("foo"), result_foo),true);
    EXPECT_EQ(foo, result_foo);
    key_list = config.getStoredKeys(num_keys);
    EXPECT_EQ(num_keys,1);
    EXPECT_EQ(strncmp("foo", key_list[0].keyBuffer, sizeof("foo")),0);

// Second set. (overwrite)
    foo = 999;
    EXPECT_EQ(config.setConfig("foo", strlen("foo"), foo),true);
    EXPECT_EQ(config.getConfig("foo", strlen("foo"), result_foo),true);
    EXPECT_EQ(foo, result_foo);
    key_list = config.getStoredKeys(num_keys);
    EXPECT_EQ(num_keys,1);
    EXPECT_EQ(strncmp("foo", key_list[0].keyBuffer, sizeof("foo")),0);

  // int32_t
    int32_t bar = -1000;
    int32_t result_bar = 0;
    EXPECT_EQ(config.setConfig("bar", strlen("bar"), bar),true);
    EXPECT_EQ(config.getConfig("bar", strlen("bar"), result_bar),true);
    EXPECT_EQ(config.getConfig("foo", strlen("foo"), result_foo),true);
    EXPECT_EQ(foo, result_foo);
    EXPECT_EQ(bar, result_bar);
    key_list = config.getStoredKeys(num_keys);
    EXPECT_EQ(num_keys,2);
    EXPECT_EQ(strncmp("bar", key_list[1].keyBuffer, sizeof("bar")),0);
    EXPECT_EQ(strncmp("foo", key_list[0].keyBuffer, sizeof("foo")),0);

//  float
    float baz = 3.14159;
    float result_baz = 0;
    EXPECT_EQ(config.setConfig("baz", strlen("baz"), baz),true);
    EXPECT_EQ(config.getConfig("baz", strlen("baz"), result_baz),true);
    EXPECT_EQ(baz, result_baz);
    key_list = config.getStoredKeys(num_keys);
    EXPECT_EQ(num_keys,3);
    EXPECT_EQ(strncmp("baz", key_list[2].keyBuffer, sizeof("baz")),0);
    EXPECT_EQ(strncmp("bar", key_list[1].keyBuffer, sizeof("bar")),0);
    EXPECT_EQ(strncmp("foo", key_list[0].keyBuffer, sizeof("foo")),0);

// str
    const char * silly = "The quick brown fox jumps over the lazy dog";
    char result_silly[100];
    size_t size = sizeof(result_silly);
    EXPECT_EQ(config.setConfig("silly", strlen("silly"), silly, strlen(silly)),true);
    EXPECT_EQ(config.getConfig("silly", strlen("silly"), result_silly, size),true);
    EXPECT_EQ(size, strlen(silly));
    EXPECT_EQ(strncmp(silly,result_silly,strlen(silly)),0);
    key_list = config.getStoredKeys(num_keys);
    EXPECT_EQ(num_keys,4);
    EXPECT_EQ(strncmp("silly", key_list[3].keyBuffer, sizeof("silly")),0);
    EXPECT_EQ(strncmp("baz", key_list[2].keyBuffer, sizeof("baz")),0);
    EXPECT_EQ(strncmp("bar", key_list[1].keyBuffer, sizeof("bar")),0);
    EXPECT_EQ(strncmp("foo", key_list[0].keyBuffer, sizeof("foo")),0);

// Bytes
    uint8_t bytes[] = {0xde,0xad,0xbe,0xef,0x5a,0xad,0xda,0xad,0xb0,0xdd};
    uint8_t result_bytes[10];
    size = sizeof(result_bytes);
    EXPECT_EQ(config.setConfig("bytes", strlen("bytes"), bytes, sizeof(bytes)),true);
    EXPECT_EQ(config.getConfig("bytes", strlen("bytes"), result_bytes, size),true);
    EXPECT_EQ(size, sizeof(bytes));
    EXPECT_EQ(memcmp(bytes,result_bytes,size),0);
    key_list = config.getStoredKeys(num_keys);
    EXPECT_EQ(num_keys,5);
    EXPECT_EQ(strncmp("bytes", key_list[4].keyBuffer, sizeof("bytes")),0);
    EXPECT_EQ(strncmp("silly", key_list[3].keyBuffer, sizeof("silly")),0);
    EXPECT_EQ(strncmp("baz", key_list[2].keyBuffer, sizeof("baz")),0);
    EXPECT_EQ(strncmp("bar", key_list[1].keyBuffer, sizeof("bar")),0);
    EXPECT_EQ(strncmp("foo", key_list[0].keyBuffer, sizeof("foo")),0);

// Remove key
    EXPECT_EQ(config.removeKey("foo", strlen("foo")), true);
    EXPECT_EQ(config.getConfig("foo", strlen("foo"), result_foo),false);
    key_list = config.getStoredKeys(num_keys);
    EXPECT_EQ(num_keys,4);
    EXPECT_EQ(strncmp("bytes", key_list[3].keyBuffer, sizeof("bytes")),0);
    EXPECT_EQ(strncmp("silly", key_list[2].keyBuffer, sizeof("silly")),0);
    EXPECT_EQ(strncmp("baz", key_list[1].keyBuffer, sizeof("baz")),0);
    EXPECT_EQ(strncmp("bar", key_list[0].keyBuffer, sizeof("bar")),0);
    EXPECT_EQ(config.getConfig("bar", strlen("bar"), result_bar),true);
    EXPECT_EQ(bar, result_bar);
    EXPECT_EQ(config.getConfig("baz", strlen("baz"), result_baz),true);
    EXPECT_EQ(baz, result_baz);
    size = sizeof(result_silly);
    EXPECT_EQ(config.getConfig("silly", strlen("silly"), result_silly, size),true);
    EXPECT_EQ(size, strlen(silly));
    EXPECT_EQ(strncmp(silly,result_silly,strlen(silly)),0);
    size = sizeof(result_bytes);
    EXPECT_EQ(config.getConfig("bytes", strlen("bytes"),result_bytes, size),true);
    EXPECT_EQ(size, sizeof(bytes));
    EXPECT_EQ(memcmp(bytes,result_bytes,size),0);
}

TEST_F(ConfigurationTest, NoKeyFound)
{
  const ext_flash_partition_t test_configuration = {
      .fa_off = 4096,
      .fa_size = 10000,
  };
  NvmPartition testPartition(_storage, test_configuration);
  Configuration config(testPartition,ram_hardware_configuration,RAM_HARDWARE_CONFIG_SIZE_BYTES);
  uint32_t result_foo = 0;
  EXPECT_EQ(config.getConfig("foo",strlen("foo") ,result_foo),false);
  uint32_t foo = 42;
  EXPECT_EQ(config.setConfig("foo",strlen("foo"), foo),true);
  EXPECT_EQ(config.getConfig("baz", strlen("baz"), result_foo),false);
}

TEST_F(ConfigurationTest, TooMuchThingy)
{
  const ext_flash_partition_t test_configuration = {
      .fa_off = 4096,
      .fa_size = 10000,
  };
  NvmPartition testPartition(_storage, test_configuration);
  Configuration config(testPartition,ram_hardware_configuration,RAM_HARDWARE_CONFIG_SIZE_BYTES);

  const char * silly = "The quick brown fox jumps over the lazy dog The quick brown fox jumps over the lazy dog";
  EXPECT_EQ(config.setConfig("silly",strlen("silly"), silly, strlen(silly)),false);
  uint8_t bytes[100];
  memset(bytes, 0xa5, sizeof(bytes));
  EXPECT_EQ(config.setConfig("bytes", strlen("bytes"), bytes, sizeof(bytes)),false);
}

TEST_F(ConfigurationTest, TooLittleStorage)
{
  const ext_flash_partition_t test_configuration = {
      .fa_off = 4096,
      .fa_size = 10000,
  };
  uint8_t small_storage;
  NvmPartition testPartition(_storage, test_configuration);
  EXPECT_DEATH(Configuration config(testPartition,&small_storage,sizeof(small_storage)), "");
}

TEST_F(ConfigurationTest, NoKeyToRemove)
{
  const ext_flash_partition_t test_configuration = {
      .fa_off = 4096,
      .fa_size = 10000,
  };
  NvmPartition testPartition(_storage, test_configuration);
  Configuration config(testPartition,ram_hardware_configuration,RAM_HARDWARE_CONFIG_SIZE_BYTES);
  EXPECT_EQ(config.removeKey("foo", strlen("foo")), false);
}

TEST_F(ConfigurationTest, cborGetSet){
    uint8_t cborBuffer[MAX_STR_LEN_BYTES];
    const ext_flash_partition_t test_configuration = {
        .fa_off = 4096,
        .fa_size = 10000,
    };
    NvmPartition testPartition(_storage, test_configuration);
    Configuration config(testPartition,ram_hardware_configuration,RAM_HARDWARE_CONFIG_SIZE_BYTES);
    uint8_t num_keys;
    const ConfigKey_t *key_list = NULL;
    key_list = config.getStoredKeys(num_keys);
    EXPECT_EQ(num_keys,0);
    EXPECT_EQ((key_list != NULL), true);

    uint32_t foo = 42;
    uint32_t result_foo = 0;
    EXPECT_EQ(config.setConfig("foo",strlen("foo"), foo),true);
    EXPECT_EQ(config.getConfig("foo",strlen("foo"), result_foo),true);
    EXPECT_EQ(foo, result_foo);
    key_list = config.getStoredKeys(num_keys);
    EXPECT_EQ(num_keys,1);
    EXPECT_EQ(strncmp("foo", key_list[0].keyBuffer, sizeof("foo")),0);
    size_t buffer_size = sizeof(cborBuffer);
    EXPECT_EQ(config.getConfigCbor("foo",strlen("foo"), cborBuffer, buffer_size), true);
    EXPECT_EQ(config.setConfigCbor("bar",strlen("foo"), cborBuffer, buffer_size), true);
    key_list = config.getStoredKeys(num_keys);
    EXPECT_EQ(num_keys,2);
    uint32_t result_bar;
    EXPECT_EQ(config.getConfig("bar",strlen("bar"), result_bar),true);
    EXPECT_EQ(foo, result_bar);
    size_t bar_size;
    EXPECT_EQ(config.getValueSize("bar",strlen("bar"), bar_size),true);
    EXPECT_EQ(bar_size, sizeof(foo));

    buffer_size = sizeof(cborBuffer);
    const char * silly = "The quick brown fox jumps over the lazy dog";
    EXPECT_EQ(config.setConfig("silly", strlen("silly"), silly, strlen(silly)),true);
    EXPECT_EQ(config.getConfigCbor("silly", strlen("silly"), cborBuffer, buffer_size),true);
    EXPECT_EQ(config.setConfigCbor("bar", strlen("bar"), cborBuffer, buffer_size), true);
    key_list = config.getStoredKeys(num_keys);
    EXPECT_EQ(num_keys,3);
    char silly_bar[100];
    size_t size = sizeof(silly_bar);
    EXPECT_EQ(config.getConfig("bar", strlen("bar"), silly_bar, size),true);
    EXPECT_EQ(strncmp(silly,silly_bar,strlen(silly)),0);
    EXPECT_EQ(config.getValueSize("bar",strlen("bar"), bar_size),true);
    EXPECT_EQ(bar_size, strlen(silly));
}

TEST_F(ConfigurationTest, BadCborGetSet){
    uint8_t cborBuffer[MAX_STR_LEN_BYTES];
    size_t buffer_size = sizeof(cborBuffer);
    memset(cborBuffer, 0xFF, buffer_size);
    const ext_flash_partition_t test_configuration = {
        .fa_off = 4096,
        .fa_size = 10000,
    };
    NvmPartition testPartition(_storage, test_configuration);
    Configuration config(testPartition,ram_hardware_configuration,RAM_HARDWARE_CONFIG_SIZE_BYTES);
    uint8_t num_keys;
    const ConfigKey_t *key_list = NULL;
    key_list = config.getStoredKeys(num_keys);
    EXPECT_EQ(num_keys,0);
    EXPECT_EQ((key_list != NULL), true);
    EXPECT_EQ(config.getConfigCbor("foo", strlen("foo"), cborBuffer, buffer_size), false); // Key doesn't exist.
    EXPECT_EQ(config.setConfigCbor("foo", strlen("foo"), cborBuffer, buffer_size), false); // Invalid cbor buffer.
    uint32_t foo = 42;
    EXPECT_EQ(config.setConfig("foo", strlen("foo"), foo),true);
    EXPECT_EQ(config.getConfigCbor("foo", strlen("foo"), cborBuffer, buffer_size), true);
    EXPECT_EQ(config.setConfigCbor("a super long key string that shouldn't work", strlen("a super long key string that shouldn't work"), cborBuffer, buffer_size), false); // key string too long.
    key_list = config.getStoredKeys(num_keys);
    EXPECT_EQ(num_keys,1);
}
