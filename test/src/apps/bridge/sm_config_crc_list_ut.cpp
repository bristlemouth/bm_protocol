#include "configuration.h"
#include "fff.h"
#include "mock_configuration.h"
#include "sm_config_crc_list.h"
#include "gtest/gtest.h"

extern "C" {
#include "bm_configs_generic.h"
#include "configuration.h"
}

DEFINE_FFF_GLOBALS;

using namespace testing;

bool bm_config_read(BmConfigPartition partition, uint32_t offset, uint8_t *buffer,
                    size_t length, uint32_t timeout_ms) {
  bool ret = false;
  (void)offset;
  (void)buffer;
  (void)length;
  (void)timeout_ms;

  switch (partition) {
  case BM_CFG_PARTITION_USER:
  case BM_CFG_PARTITION_SYSTEM:
  case BM_CFG_PARTITION_HARDWARE:
    ret = true;
    break;
  default:
    break;
  }

  return ret;
}
bool bm_config_write(BmConfigPartition partition, uint32_t offset, uint8_t *buffer,
                     size_t length, uint32_t timeout_ms) {
  bool ret = false;
  (void)offset;
  (void)buffer;
  (void)length;
  (void)timeout_ms;

  switch (partition) {
  case BM_CFG_PARTITION_USER:
  case BM_CFG_PARTITION_SYSTEM:
  case BM_CFG_PARTITION_HARDWARE:
    ret = true;
    break;
  default:
    break;
  }

  return ret;
}
void bm_config_reset(void) {}

TEST(SMConfigCRCListTests, EmptyList) {
  config_init();
  uint8_t buf[] = {0x80}; // empty cbor array
  uint32_t buf_len = sizeof(buf);
  SMConfigCRCList sm_config_crc_list(BM_CFG_PARTITION_SYSTEM);
  set_config_cbor(BM_CFG_PARTITION_SYSTEM, SMConfigCRCList::KEY, SMConfigCRCList::KEY_LEN, buf,
                  buf_len);
  EXPECT_EQ(sm_config_crc_list.contains(0x12345678), false);
  uint32_t list_size = 0;
  uint32_t *crc_list = sm_config_crc_list.alloc_list(list_size);
  EXPECT_EQ(list_size, 0);
  EXPECT_EQ(crc_list, nullptr);
}

TEST(SMConfigCRCListTests, OneItemList) {
  uint8_t buf[] = {0x81, 0x1A, 0x12, 0x34, 0x56, 0x78};
  uint32_t buf_len = sizeof(buf);

  config_init();
  SMConfigCRCList sm_config_crc_list(BM_CFG_PARTITION_SYSTEM);
  set_config_cbor(BM_CFG_PARTITION_SYSTEM, SMConfigCRCList::KEY, SMConfigCRCList::KEY_LEN, buf,
                  buf_len);
  EXPECT_EQ(sm_config_crc_list.contains(0x78563412), false);
  EXPECT_EQ(sm_config_crc_list.contains(0x12345678), true);
}

TEST(SMConfigCRCListTests, AddOneItem) {
  uint8_t buf[SMConfigCRCList::MAX_BUFFER_SIZE] = {0x81, 0x1A, 0x12, 0x34, 0x56, 0x78};
  size_t buf_size = 6;

  config_init();
  SMConfigCRCList sm_config_crc_list(BM_CFG_PARTITION_SYSTEM);
  set_config_cbor(BM_CFG_PARTITION_SYSTEM, SMConfigCRCList::KEY, SMConfigCRCList::KEY_LEN, buf,
                  buf_size);
  EXPECT_TRUE(sm_config_crc_list.contains(0x12345678));
  EXPECT_FALSE(sm_config_crc_list.contains(0x78563412));
  EXPECT_FALSE(sm_config_crc_list.contains(0x21436587));
  EXPECT_FALSE(sm_config_crc_list.contains(0x87654321));
  sm_config_crc_list.add(0x87654321);
  EXPECT_TRUE(sm_config_crc_list.contains(0x87654321));
  EXPECT_TRUE(sm_config_crc_list.contains(0x12345678));
}

TEST(SMConfigCRCListTests, AddMoreItemsThanListMax) {
  uint8_t buf[SMConfigCRCList::MAX_BUFFER_SIZE] = {0x80};
  size_t buf_size = 1;

  config_init();
  SMConfigCRCList sm_config_crc_list(BM_CFG_PARTITION_SYSTEM);
  set_config_cbor(BM_CFG_PARTITION_SYSTEM, SMConfigCRCList::KEY, SMConfigCRCList::KEY_LEN, buf,
                  buf_size);
  // Fill the list and check that they are all there
  constexpr uint32_t crc_start = 0xF0000001;
  constexpr uint32_t crc_endcap = crc_start + SMConfigCRCList::MAX_LIST_SIZE;
  uint32_t crc;
  for (crc = crc_start; crc < crc_endcap; crc++) {
    sm_config_crc_list.add(crc);
  }
  for (crc = crc_start; crc < crc_endcap; crc++) {
    EXPECT_TRUE(sm_config_crc_list.contains(crc));
  }

  // Add one more and check that the first one is gone
  sm_config_crc_list.add(crc_endcap);
  EXPECT_FALSE(sm_config_crc_list.contains(crc_start));
  for (crc = crc_start + 1; crc <= crc_endcap; crc++) {
    EXPECT_TRUE(sm_config_crc_list.contains(crc));
  }
}

TEST(SMConfigCRCListTests, Clear) {
  uint8_t buf[] = {0x81, 0x1A, 0x12, 0x34, 0x56, 0x78};
  uint32_t buf_len = sizeof(buf);

  config_init();
  SMConfigCRCList sm_config_crc_list(BM_CFG_PARTITION_SYSTEM);
  set_config_cbor(BM_CFG_PARTITION_SYSTEM, SMConfigCRCList::KEY, SMConfigCRCList::KEY_LEN, buf,
                  buf_len);
  sm_config_crc_list.clear();
}

TEST(SMConfigCRCListTests, AllocList) {
  uint8_t buf[] = {0x81, 0x1A, 0x12, 0x34, 0x56, 0x78};
  uint32_t buf_len = sizeof(buf);

  config_init();
  SMConfigCRCList sm_config_crc_list(BM_CFG_PARTITION_SYSTEM);
  set_config_cbor(BM_CFG_PARTITION_SYSTEM, SMConfigCRCList::KEY, SMConfigCRCList::KEY_LEN, buf,
                  buf_len);
  EXPECT_EQ(sm_config_crc_list.contains(0x12345678), true);
  uint32_t list_size;
  uint32_t *crc_list = sm_config_crc_list.alloc_list(list_size);
  EXPECT_EQ(list_size, 1);
  EXPECT_EQ(crc_list[0], 0x12345678);
  free(crc_list);
}
