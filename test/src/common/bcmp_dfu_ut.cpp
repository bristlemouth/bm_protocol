#include "gtest/gtest.h"

#include "bm_dfu.h"
#include "fff.h"
#include "mock_device_info.h"
#include "nvmPartition.h"
#include "mock_storage_driver.h"
extern "C" {
#include "mock_FreeRTOS.h"
}

DEFINE_FFF_GLOBALS;

using namespace testing;

FAKE_VALUE_FUNC(bool, fake_bcmp_tx_func, bcmp_message_type_t, uint8_t *, uint16_t);

// The fixture for testing class Foo.
class BcmpDfuTest : public ::testing::Test {
 protected:
  // You can remove any or all of the following functions if its body
  // is empty.

  BcmpDfuTest() {
     // You can do set-up work for each test here.
  }

  ~BcmpDfuTest() override {
     // You can do clean-up work that doesn't throw exceptions here.
  }

  // If the constructor and destructor are not enough for setting up
  // and cleaning up each test, you can define the following methods:

  void SetUp() override {
     // Code here will be called immediately after the constructor (right
     // before each test).
        RESET_FAKE(xQueueGenericSend);
        RESET_FAKE(xQueueGenericCreate);
        RESET_FAKE(xTaskCreate);
        RESET_FAKE(xTimerCreate);
        RESET_FAKE(getNodeId);
        RESET_FAKE(fake_bcmp_tx_func);
        fake_q = (QueueHandle_t)malloc(sizeof(QueueHandle_t));
        fake_timer = (TimerHandle_t)malloc(sizeof(QueueHandle_t));
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
        testPartition = new NvmPartition(_storage, _test_configuration);
    }

  void TearDown() override {
     // Code here will be called immediately after each test (right
     // before the destructor).
        free(fake_q);
        delete testPartition;
  }

  // Objects declared here can be used by all tests in the test suite for Foo.
    QueueHandle_t fake_q;
    TimerHandle_t fake_timer;
    MockStorageDriver _storage;
    NvmPartition *testPartition;
    const ext_flash_partition_t _test_configuration = {
        .fa_off = 4096,
        .fa_size = 10000,
    };
};

TEST_F(BcmpDfuTest, InitTest)
{
    xQueueGenericSend_fake.return_val = pdPASS;
    xTaskCreate_fake.return_val = pdPASS;
    xQueueGenericCreate_fake.return_val = fake_q;
    xTimerCreate_fake.return_val = fake_timer;
    getNodeId_fake.return_val = 0xdeadbeefbeeffeed;
    bm_dfu_init(fake_bcmp_tx_func, testPartition);
    EXPECT_EQ(xQueueGenericSend_fake.call_count, 1);
    EXPECT_EQ(xTaskCreate_fake.call_count, 1);
    EXPECT_EQ(xQueueGenericCreate_fake.call_count, 1);
    EXPECT_EQ(xTimerCreate_fake.call_count, 3);
    EXPECT_EQ(getNodeId_fake.call_count, 3);
}

TEST_F(BcmpDfuTest, processMessageTest)
{
    xQueueGenericSend_fake.return_val = pdPASS;
    xTaskCreate_fake.return_val = pdPASS;
    xQueueGenericCreate_fake.return_val = fake_q;
    xTimerCreate_fake.return_val = fake_timer;
    getNodeId_fake.return_val = 0xdeadbeefbeeffeed;
    bm_dfu_init(fake_bcmp_tx_func, testPartition);
    EXPECT_EQ(xQueueGenericSend_fake.call_count, 1);
    EXPECT_EQ(xTaskCreate_fake.call_count, 1);
    EXPECT_EQ(xQueueGenericCreate_fake.call_count, 1);
    EXPECT_EQ(xTimerCreate_fake.call_count, 3);
    EXPECT_EQ(getNodeId_fake.call_count, 3);

    // dfu start
    bcmp_dfu_start_t* start_msg = (bcmp_dfu_start_t*) malloc (sizeof(bcmp_dfu_start_t));
    start_msg->header.frame_type = BCMP_DFU_START;
    start_msg->info.addresses.dst_node_id = 0xdeadbeefbeeffeed;
    start_msg->info.addresses.src_node_id = 0xdeaddeaddeaddead;
    bm_dfu_process_message((uint8_t*)start_msg, sizeof(bcmp_dfu_start_t));
    EXPECT_EQ(xQueueGenericSend_fake.call_count, 2);
    free(start_msg);

    // payload req
    bcmp_dfu_payload_req_t* payload_req_msg = (bcmp_dfu_payload_req_t*) malloc (sizeof(bcmp_dfu_payload_req_t));
    payload_req_msg->header.frame_type = BCMP_DFU_PAYLOAD_REQ;
    payload_req_msg->chunk_req.addresses.dst_node_id = 0xdeadbeefbeeffeed;
    payload_req_msg->chunk_req.addresses.src_node_id = 0xdeaddeaddeaddead;
    bm_dfu_process_message((uint8_t*)payload_req_msg, sizeof(bcmp_dfu_payload_req_t));
    EXPECT_EQ(xQueueGenericSend_fake.call_count, 3);
    free(payload_req_msg);

    // payload
    bcmp_dfu_payload_t* payload_msg = (bcmp_dfu_payload_t*) malloc (sizeof(bcmp_dfu_payload_t));
    payload_msg->header.frame_type = BCMP_DFU_PAYLOAD;
    payload_msg->chunk.addresses.dst_node_id = 0xdeadbeefbeeffeed;
    payload_msg->chunk.addresses.src_node_id = 0xdeaddeaddeaddead;
    bm_dfu_process_message((uint8_t*)payload_msg, sizeof(bcmp_dfu_payload_t));
    EXPECT_EQ(xQueueGenericSend_fake.call_count, 4);
    free(payload_msg);

    // dfu end
    bcmp_dfu_end_t* end_msg = (bcmp_dfu_end_t*) malloc (sizeof(bcmp_dfu_end_t));
    end_msg->header.frame_type = BCMP_DFU_END;
    end_msg->result.addresses.dst_node_id = 0xdeadbeefbeeffeed;
    end_msg->result.addresses.src_node_id = 0xdeaddeaddeaddead;
    bm_dfu_process_message((uint8_t*)end_msg, sizeof(bcmp_dfu_end_t));
    EXPECT_EQ(xQueueGenericSend_fake.call_count, 5);
    free(end_msg);

    // dfu ack
    bcmp_dfu_ack_t* ack_msg = (bcmp_dfu_ack_t*) malloc (sizeof(bcmp_dfu_ack_t));
    ack_msg->header.frame_type = BCMP_DFU_ACK;
    ack_msg->ack.addresses.dst_node_id = 0xdeadbeefbeeffeed;
    ack_msg->ack.addresses.src_node_id = 0xdeaddeaddeaddead;
    bm_dfu_process_message((uint8_t*)ack_msg, sizeof(bcmp_dfu_ack_t));
    EXPECT_EQ(xQueueGenericSend_fake.call_count, 6);
    free(ack_msg);
    // dfu abort
    bcmp_dfu_abort_t* abort_msg = (bcmp_dfu_abort_t*) malloc (sizeof(bcmp_dfu_abort_t));
    abort_msg->header.frame_type = BCMP_DFU_ABORT;
    abort_msg->err.addresses.dst_node_id = 0xdeadbeefbeeffeed;
    abort_msg->err.addresses.src_node_id = 0xdeaddeaddeaddead;
    bm_dfu_process_message((uint8_t*)abort_msg, sizeof(bcmp_dfu_abort_t));
    EXPECT_EQ(xQueueGenericSend_fake.call_count, 7);
    free(abort_msg);
    // dfu heartbeat
    bcmp_dfu_heartbeat_t* hb_msg = (bcmp_dfu_heartbeat_t*) malloc (sizeof(bcmp_dfu_heartbeat_t));
    hb_msg->header.frame_type = BCMP_DFU_HEARTBEAT;
    hb_msg->addr.dst_node_id = 0xdeadbeefbeeffeed;
    hb_msg->addr.src_node_id = 0xdeaddeaddeaddead;
    bm_dfu_process_message((uint8_t*)hb_msg, sizeof(bcmp_dfu_heartbeat_t));
    EXPECT_EQ(xQueueGenericSend_fake.call_count, 8);
    free(hb_msg);
    // Invalid msg
    bcmp_dfu_heartbeat_t* bad_msg = (bcmp_dfu_heartbeat_t*) malloc (sizeof(bcmp_dfu_heartbeat_t));
    bad_msg->header.frame_type = BCMP_CONFIG_COMMIT;
    bad_msg->addr.dst_node_id = 0xdeadbeefbeeffeed;
    bad_msg->addr.src_node_id = 0xdeaddeaddeaddead;
    EXPECT_DEATH(bm_dfu_process_message((uint8_t*)bad_msg, sizeof(bcmp_dfu_heartbeat_t)),"");
    free(bad_msg);
}

TEST_F(BcmpDfuTest, DfuApiTest){
    xQueueGenericSend_fake.return_val = pdPASS;
    xTaskCreate_fake.return_val = pdPASS;
    xQueueGenericCreate_fake.return_val = fake_q;
    xTimerCreate_fake.return_val = fake_timer;
    getNodeId_fake.return_val = 0xdeadbeefbeeffeed;
    bm_dfu_init(fake_bcmp_tx_func, testPartition);
    EXPECT_EQ(xQueueGenericSend_fake.call_count, 1);
    EXPECT_EQ(xTaskCreate_fake.call_count, 1);
    EXPECT_EQ(xQueueGenericCreate_fake.call_count, 1);
    EXPECT_EQ(xTimerCreate_fake.call_count, 3);
    EXPECT_EQ(getNodeId_fake.call_count, 3);

    bm_dfu_send_ack(0xdeadbeefbeeffeed, 1, BM_DFU_ERR_NONE);
    EXPECT_EQ(fake_bcmp_tx_func_fake.call_count, 1);
    EXPECT_EQ(fake_bcmp_tx_func_fake.arg0_val, BCMP_DFU_ACK);

    bm_dfu_req_next_chunk(0xdeadbeefbeeffeed, 0);
    EXPECT_EQ(fake_bcmp_tx_func_fake.call_count, 2);
    EXPECT_EQ(fake_bcmp_tx_func_fake.arg0_val, BCMP_DFU_PAYLOAD_REQ);

    bm_dfu_update_end(0xdeadbeefbeeffeed, 1, BM_DFU_ERR_NONE);
    EXPECT_EQ(fake_bcmp_tx_func_fake.call_count, 3);
    EXPECT_EQ(fake_bcmp_tx_func_fake.arg0_val, BCMP_DFU_END);

    bm_dfu_send_heartbeat(0xdeadbeefbeeffeed);
    EXPECT_EQ(fake_bcmp_tx_func_fake.call_count, 4);
    EXPECT_EQ(fake_bcmp_tx_func_fake.arg0_val, BCMP_DFU_HEARTBEAT);

    bm_dfu_img_info_t info;
    info.chunk_size = BM_DFU_MAX_CHUNK_SIZE;
    info.crc16 = 0xbaad;
    info.image_size = 2 * 1000 * 1024;
    info.major_ver = 0;
    info.minor_ver = 1; 
    EXPECT_EQ(bm_dfu_initiate_update(info,0xdeadbeefbeeffeed), true);
    EXPECT_EQ(xQueueGenericSend_fake.call_count, 2);
    info.chunk_size = 2048;
    EXPECT_EQ(bm_dfu_initiate_update(info,0xdeadbeefbeeffeed), false);
}
