#include "fff.h"
#include "mock_configuration.h"
#include "sm_config_crc_list.h"
#include "gtest/gtest.h"

DEFINE_FFF_GLOBALS;

using namespace testing;

TEST(SMConfigCRCListTests, EmptyList) {
  MockConfiguration mock_cfg;
  ON_CALL(mock_cfg,
          getConfigCbor(StrEq(SMConfigCRCList::KEY), Eq(SMConfigCRCList::KEY_LEN), _, _))
      .WillByDefault(
          Invoke([](const char *key, size_t key_len, uint8_t *value, size_t &value_len) {
            (void)key;
            (void)key_len;
            uint8_t buf[] = {0x80}; // empty cbor array
            value_len = sizeof(buf);
            memcpy(value, buf, value_len);
            return true;
          }));

  SMConfigCRCList sm_config_crc_list(&mock_cfg);
  EXPECT_EQ(sm_config_crc_list.contains(0x12345678), false);
}

TEST(SMConfigCRCListTests, OneItemList) {
  MockConfiguration mock_cfg;
  ON_CALL(mock_cfg,
          getConfigCbor(StrEq(SMConfigCRCList::KEY), Eq(SMConfigCRCList::KEY_LEN), _, _))
      .WillByDefault(
          Invoke([](const char *key, size_t key_len, uint8_t *value, size_t &value_len) {
            (void)key;
            (void)key_len;
            // cbor array containing one uint32
            uint8_t buf[] = {0x81, 0x1A, 0x12, 0x34, 0x56, 0x78};
            value_len = sizeof(buf);
            memcpy(value, buf, value_len);
            return true;
          }));

  SMConfigCRCList sm_config_crc_list(&mock_cfg);
  EXPECT_EQ(sm_config_crc_list.contains(0x78563412), false);
  EXPECT_EQ(sm_config_crc_list.contains(0x12345678), true);
}

TEST(SMConfigCRCListTests, AddOneItem) {
  uint8_t buf[SMConfigCRCList::MAX_BUFFER_SIZE] = {0x81, 0x1A, 0x12, 0x34, 0x56, 0x78};
  size_t buf_size = 6;

  MockConfiguration mock_cfg;
  ON_CALL(mock_cfg,
          getConfigCbor(StrEq(SMConfigCRCList::KEY), Eq(SMConfigCRCList::KEY_LEN), _, _))
      .WillByDefault(Invoke(
          [&buf, &buf_size](const char *key, size_t key_len, uint8_t *value, size_t &value_len) {
            (void)key;
            (void)key_len;
            value_len = buf_size;
            memcpy(value, buf, value_len);
            return true;
          }));
  EXPECT_CALL(mock_cfg,
              setConfigCbor(StrEq(SMConfigCRCList::KEY), Eq(SMConfigCRCList::KEY_LEN), _, _))
      .WillOnce(Invoke(
          [&buf, &buf_size](const char *key, size_t key_len, uint8_t *value, size_t value_len) {
            (void)key;
            (void)key_len;
            buf_size = value_len;
            memcpy(buf, value, value_len);
            return true;
          }));

  SMConfigCRCList sm_config_crc_list(&mock_cfg);
  EXPECT_TRUE(sm_config_crc_list.contains(0x12345678));
  EXPECT_FALSE(sm_config_crc_list.contains(0x78563412));
  EXPECT_FALSE(sm_config_crc_list.contains(0x21436587));
  EXPECT_FALSE(sm_config_crc_list.contains(0x87654321));
  sm_config_crc_list.add(0x87654321);
  EXPECT_TRUE(sm_config_crc_list.contains(0x87654321));
  EXPECT_TRUE(sm_config_crc_list.contains(0x12345678));
}
