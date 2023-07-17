#include "gtest/gtest.h"

#include "bm_dfu.h"
#include "fff.h"
#include "mock_device_info.h"
#include "nvmPartition.h"
#include "mock_storage_driver.h"
#include "flash_map_backend.h"
#include "mock_reset_reason.h"
#include "mock_timer_callback_handler.h"
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
        RESET_FAKE(xTimerGenericCommand);
        RESET_FAKE(getVersionInfo);
        RESET_FAKE(resetSystem);
        RESET_FAKE(getGitSHA);
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
        xQueueGenericSend_fake.return_val = pdPASS;
        xTaskCreate_fake.return_val = pdPASS;
        xQueueGenericCreate_fake.return_val = fake_q;
        xTimerCreate_fake.return_val = fake_timer;
        xTimerGenericCommand_fake.return_val = pdPASS;
        getNodeId_fake.return_val = 0xdeadbeefbeeffeed;
        fake_bcmp_tx_func_fake.return_val = true;
        getVersionInfo_fake.return_val = &test_info;
        getGitSHA_fake.return_val = 0xd00dd00d;
        memset(&client_update_reboot_info, 0, sizeof(client_update_reboot_info));
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
    const struct flash_area fa = {
        .fa_id = 1,
        .fa_device_id = 2,
        .pad16 = 0,
        .fa_off = 0,
        .fa_size = 2000000,
    };
    const versionInfo_t test_info = {
        .magic = 0xbaadd00dd00dbaad,
        .gitSHA = 0xd00dd00d,
        .maj = 1,
        .min = 7,
        .rev = 0,
        .hwVersion = 1,
        .flags = 0,
        .versionStrLen = 5,
        .versionStr = {"test"},
    };
    static constexpr size_t CHUNK_SIZE = 512;
    static constexpr size_t IMAGE_SIZE = 2048;
};

TEST_F(BcmpDfuTest, InitTest)
{
    bm_dfu_init(fake_bcmp_tx_func, testPartition);
    EXPECT_EQ(xQueueGenericSend_fake.call_count, 1);
    EXPECT_EQ(xTaskCreate_fake.call_count, 1);
    EXPECT_EQ(xQueueGenericCreate_fake.call_count, 1);
    EXPECT_EQ(xTimerCreate_fake.call_count, 4);
    EXPECT_EQ(getNodeId_fake.call_count, 3);
}

TEST_F(BcmpDfuTest, processMessageTest)
{
    bm_dfu_init(fake_bcmp_tx_func, testPartition);
    EXPECT_EQ(xQueueGenericSend_fake.call_count, 1);
    EXPECT_EQ(xTaskCreate_fake.call_count, 1);
    EXPECT_EQ(xQueueGenericCreate_fake.call_count, 1);
    EXPECT_EQ(xTimerCreate_fake.call_count, 4);
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
    bm_dfu_init(fake_bcmp_tx_func, testPartition);
    EXPECT_EQ(xQueueGenericSend_fake.call_count, 1);
    EXPECT_EQ(xTaskCreate_fake.call_count, 1);
    EXPECT_EQ(xQueueGenericCreate_fake.call_count, 1);
    EXPECT_EQ(xTimerCreate_fake.call_count, 4);
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
    info.gitSHA = 0xd00dd00d;
    EXPECT_EQ(bm_dfu_initiate_update(info,0xdeadbeefbeeffeed, NULL, 1000), false);
}

TEST_F(BcmpDfuTest, clientGolden) {
    getGitSHA_fake.return_val = 0xbaaddaad;
    bm_dfu_test_set_client_fa(&fa);

    // INIT SUCCESS
    bm_dfu_init(fake_bcmp_tx_func, testPartition);
    libSmContext_t* ctx = bm_dfu_test_get_sm_ctx();
    bm_dfu_event_t evt = {
        .type = DFU_EVENT_INIT_SUCCESS,
        .buf = NULL,
        .len = 0,
    };
    bm_dfu_test_set_dfu_event_and_run_sm(evt);
    EXPECT_EQ(getCurrentStateEnum(*ctx), BM_DFU_STATE_IDLE);

    // DFU REQUEST
    evt.type = DFU_EVENT_RECEIVED_UPDATE_REQUEST;
    evt.buf = (uint8_t*)malloc(sizeof(bcmp_dfu_start_t));
    evt.len = sizeof(bcmp_dfu_start_t);
    bcmp_dfu_start_t dfu_start_msg;
    dfu_start_msg.header.frame_type = BCMP_DFU_START;
    dfu_start_msg.info.addresses.src_node_id = 0xbeefbeefdaadbaad;
    dfu_start_msg.info.addresses.dst_node_id = 0xdeadbeefbeeffeed;
    dfu_start_msg.info.img_info.image_size = IMAGE_SIZE;
    dfu_start_msg.info.img_info.chunk_size = CHUNK_SIZE;
    dfu_start_msg.info.img_info.crc16 = 0x2fDf; 
    dfu_start_msg.info.img_info.major_ver = 1;
    dfu_start_msg.info.img_info.minor_ver = 7;
    dfu_start_msg.info.img_info.gitSHA = 0xdeadd00d;
    memcpy(evt.buf, &dfu_start_msg, sizeof(bcmp_dfu_start_t));

    bm_dfu_test_set_dfu_event_and_run_sm(evt);
    EXPECT_EQ(fake_bcmp_tx_func_fake.arg0_history[0], BCMP_DFU_ACK);
    EXPECT_EQ(fake_bcmp_tx_func_fake.arg0_val, BCMP_DFU_PAYLOAD_REQ);
    EXPECT_EQ(getCurrentStateEnum(*ctx), BM_DFU_STATE_CLIENT_RECEIVING);

    // Chunk
    evt.type = DFU_EVENT_IMAGE_CHUNK;
    evt.buf = (uint8_t*)malloc(sizeof(bcmp_dfu_payload_t) + CHUNK_SIZE);
    evt.len = sizeof(bcmp_dfu_payload_t) + CHUNK_SIZE;
    bcmp_dfu_payload_t dfu_payload_msg;
    dfu_payload_msg.header.frame_type = BCMP_DFU_PAYLOAD;
    dfu_payload_msg.chunk.addresses.src_node_id = 0xbeefbeefdaadbaad;
    dfu_payload_msg.chunk.addresses.dst_node_id = 0xdeadbeefbeeffeed;
    dfu_payload_msg.chunk.payload_length = CHUNK_SIZE;
    memcpy(evt.buf, &dfu_payload_msg, sizeof(dfu_payload_msg));
    memset(evt.buf+sizeof(dfu_payload_msg),0xa5,CHUNK_SIZE);
    bm_dfu_test_set_dfu_event_and_run_sm(evt); // 512
    EXPECT_EQ(fake_bcmp_tx_func_fake.arg0_val, BCMP_DFU_PAYLOAD_REQ);
    EXPECT_EQ(getCurrentStateEnum(*ctx), BM_DFU_STATE_CLIENT_RECEIVING); 
    bm_dfu_test_set_dfu_event_and_run_sm(evt); // 1024
    EXPECT_EQ(fake_bcmp_tx_func_fake.arg0_val, BCMP_DFU_PAYLOAD_REQ);
    EXPECT_EQ(getCurrentStateEnum(*ctx), BM_DFU_STATE_CLIENT_RECEIVING);
    bm_dfu_test_set_dfu_event_and_run_sm(evt); // 1536
    EXPECT_EQ(fake_bcmp_tx_func_fake.arg0_val, BCMP_DFU_PAYLOAD_REQ);
    EXPECT_EQ(getCurrentStateEnum(*ctx), BM_DFU_STATE_CLIENT_RECEIVING);
    bm_dfu_test_set_dfu_event_and_run_sm(evt); // 2048
    EXPECT_EQ(getCurrentStateEnum(*ctx), BM_DFU_STATE_CLIENT_VALIDATING);

    // Validating
    evt.type = DFU_EVENT_NONE;
    evt.buf = NULL;
    evt.len = 0;
    bm_dfu_test_set_dfu_event_and_run_sm(evt); 
    EXPECT_EQ(getCurrentStateEnum(*ctx), BM_DFU_STATE_CLIENT_REBOOT_REQ);
    EXPECT_EQ(fake_bcmp_tx_func_fake.arg0_val, BCMP_DFU_REBOOT_REQ);

    // Reboot
    evt.type = DFU_EVENT_REBOOT;
    evt.buf = (uint8_t*)malloc(sizeof(bcmp_dfu_reboot_t));
    evt.len = sizeof(bcmp_dfu_reboot_t);
    bcmp_dfu_reboot_t dfu_reboot_msg;
    dfu_reboot_msg.header.frame_type = BCMP_DFU_REBOOT;
    dfu_reboot_msg.addr.src_node_id = 0xbeefbeefdaadbaad;
    dfu_reboot_msg.addr.dst_node_id = 0xdeadbeefbeeffeed;
    memcpy(evt.buf, &dfu_reboot_msg, sizeof(dfu_reboot_msg));
    bm_dfu_test_set_dfu_event_and_run_sm(evt);
    EXPECT_EQ(getCurrentStateEnum(*ctx), BM_DFU_STATE_CLIENT_ACTIVATING); // We reboot in this step. 
    // See ClientImageHasUpdated for state behavior after reboot.
}

TEST_F(BcmpDfuTest, clientRejectSameSHA) {
    getGitSHA_fake.return_val = 0xdeadd00d; // same SHA
    bm_dfu_test_set_client_fa(&fa);

    // INIT SUCCESS
    bm_dfu_init(fake_bcmp_tx_func, testPartition);
    libSmContext_t* ctx = bm_dfu_test_get_sm_ctx();
    bm_dfu_event_t evt = {
        .type = DFU_EVENT_INIT_SUCCESS,
        .buf = NULL,
        .len = 0,
    };
    bm_dfu_test_set_dfu_event_and_run_sm(evt);
    EXPECT_EQ(getCurrentStateEnum(*ctx), BM_DFU_STATE_IDLE);

    // DFU REQUEST
    evt.type = DFU_EVENT_RECEIVED_UPDATE_REQUEST;
    evt.buf = (uint8_t*)malloc(sizeof(bcmp_dfu_start_t));
    evt.len = sizeof(bcmp_dfu_start_t);
    bcmp_dfu_start_t dfu_start_msg;
    dfu_start_msg.header.frame_type = BCMP_DFU_START;
    dfu_start_msg.info.addresses.src_node_id = 0xbeefbeefdaadbaad;
    dfu_start_msg.info.addresses.dst_node_id = 0xdeadbeefbeeffeed;
    dfu_start_msg.info.img_info.image_size = IMAGE_SIZE;
    dfu_start_msg.info.img_info.chunk_size = CHUNK_SIZE;
    dfu_start_msg.info.img_info.crc16 = 0x2fDf; 
    dfu_start_msg.info.img_info.major_ver = 1;
    dfu_start_msg.info.img_info.minor_ver = 7;
    dfu_start_msg.info.img_info.gitSHA = 0xdeadd00d;
    memcpy(evt.buf, &dfu_start_msg, sizeof(bcmp_dfu_start_t));

    bm_dfu_test_set_dfu_event_and_run_sm(evt);
    EXPECT_EQ(fake_bcmp_tx_func_fake.arg0_val, BCMP_DFU_ACK);
    EXPECT_EQ(getCurrentStateEnum(*ctx), BM_DFU_STATE_IDLE); // We don't progress to RECEIVING.
}

TEST_F(BcmpDfuTest, clientForceUpdate) {
    getGitSHA_fake.return_val = 0xdeadd00d; // same SHA
    bm_dfu_test_set_client_fa(&fa);

    // INIT SUCCESS
    bm_dfu_init(fake_bcmp_tx_func, testPartition);
    libSmContext_t* ctx = bm_dfu_test_get_sm_ctx();
    bm_dfu_event_t evt = {
        .type = DFU_EVENT_INIT_SUCCESS,
        .buf = NULL,
        .len = 0,
    };
    bm_dfu_test_set_dfu_event_and_run_sm(evt);
    EXPECT_EQ(getCurrentStateEnum(*ctx), BM_DFU_STATE_IDLE);

    // DFU REQUEST
    evt.type = DFU_EVENT_RECEIVED_UPDATE_REQUEST;
    evt.buf = (uint8_t*)malloc(sizeof(bcmp_dfu_start_t));
    evt.len = sizeof(bcmp_dfu_start_t);
    bcmp_dfu_start_t dfu_start_msg;
    dfu_start_msg.header.frame_type = BCMP_DFU_START;
    dfu_start_msg.info.addresses.src_node_id = 0xbeefbeefdaadbaad;
    dfu_start_msg.info.addresses.dst_node_id = 0xdeadbeefbeeffeed;
    dfu_start_msg.info.img_info.image_size = IMAGE_SIZE;
    dfu_start_msg.info.img_info.chunk_size = CHUNK_SIZE;
    dfu_start_msg.info.img_info.crc16 = 0x2fDf; 
    dfu_start_msg.info.img_info.major_ver = 1;
    dfu_start_msg.info.img_info.minor_ver = 7;
    dfu_start_msg.info.img_info.gitSHA = 0xdeadd00d;
    dfu_start_msg.info.img_info.filter_key = BM_DFU_IMG_INFO_FORCE_UPDATE; // forced update 
    memcpy(evt.buf, &dfu_start_msg, sizeof(bcmp_dfu_start_t));

    bm_dfu_test_set_dfu_event_and_run_sm(evt);
    EXPECT_EQ(fake_bcmp_tx_func_fake.arg0_history[0], BCMP_DFU_ACK);
    EXPECT_EQ(fake_bcmp_tx_func_fake.arg0_val, BCMP_DFU_PAYLOAD_REQ);
    EXPECT_EQ(getCurrentStateEnum(*ctx), BM_DFU_STATE_CLIENT_RECEIVING);
}

TEST_F(BcmpDfuTest, clientGoldenImageHasUpdated) {
    // Set the reboot info.
    client_update_reboot_info.magic = DFU_REBOOT_MAGIC;
    client_update_reboot_info.host_node_id = 0xbeefbeefdaadbaad;
    client_update_reboot_info.major = 1;
    client_update_reboot_info.minor = 7;
    client_update_reboot_info.gitSHA = 0xdeadd00d;
    getGitSHA_fake.return_val = 0xdeadd00d;

    bm_dfu_test_set_client_fa(&fa);

    // INIT SUCCESS
    bm_dfu_init(fake_bcmp_tx_func, testPartition);
    libSmContext_t* ctx = bm_dfu_test_get_sm_ctx();
    bm_dfu_event_t evt = {
        .type = DFU_EVENT_INIT_SUCCESS,
        .buf = NULL,
        .len = 0,
    };
    bm_dfu_test_set_dfu_event_and_run_sm(evt);
    EXPECT_EQ(getCurrentStateEnum(*ctx), BM_DFU_STATE_CLIENT_REBOOT_DONE);
    EXPECT_EQ(fake_bcmp_tx_func_fake.arg0_val, BCMP_DFU_BOOT_COMPLETE);

    // REBOOT_DONE
    evt.type = DFU_EVENT_UPDATE_END;
    evt.buf = (uint8_t*)malloc(sizeof(bcmp_dfu_end_t));
    evt.len = sizeof(bcmp_dfu_end_t);
    bcmp_dfu_end_t bcmp_end_msg;
    bcmp_end_msg.header.frame_type = BCMP_DFU_END;
    bcmp_end_msg.result.addresses.dst_node_id = 0xdeadbeefbeeffeed;
    bcmp_end_msg.result.addresses.src_node_id = 0xbeefbeefdaadbaad;
    bcmp_end_msg.result.err_code = BM_DFU_ERR_NONE;
    bcmp_end_msg.result.success = 1;
    memcpy(evt.buf, &bcmp_end_msg, sizeof(bcmp_end_msg));
    bm_dfu_test_set_dfu_event_and_run_sm(evt);
    EXPECT_EQ(getCurrentStateEnum(*ctx), BM_DFU_STATE_IDLE);
    EXPECT_EQ(fake_bcmp_tx_func_fake.arg0_val, BCMP_DFU_END);
}

TEST_F(BcmpDfuTest, clientResyncHost) {
    bm_dfu_test_set_client_fa(&fa);

    // INIT SUCCESS
    bm_dfu_init(fake_bcmp_tx_func, testPartition);
    libSmContext_t* ctx = bm_dfu_test_get_sm_ctx();
    bm_dfu_event_t evt = {
        .type = DFU_EVENT_INIT_SUCCESS,
        .buf = NULL,
        .len = 0,
    };
    bm_dfu_test_set_dfu_event_and_run_sm(evt);
    EXPECT_EQ(getCurrentStateEnum(*ctx), BM_DFU_STATE_IDLE);

    // DFU REQUEST
    evt.type = DFU_EVENT_RECEIVED_UPDATE_REQUEST;
    evt.buf = (uint8_t*)malloc(sizeof(bcmp_dfu_start_t));
    evt.len = sizeof(bcmp_dfu_start_t);
    bcmp_dfu_start_t dfu_start_msg;
    dfu_start_msg.header.frame_type = BCMP_DFU_START;
    dfu_start_msg.info.addresses.src_node_id = 0xbeefbeefdaadbaad;
    dfu_start_msg.info.addresses.dst_node_id = 0xdeadbeefbeeffeed;
    dfu_start_msg.info.img_info.image_size = IMAGE_SIZE;
    dfu_start_msg.info.img_info.chunk_size = CHUNK_SIZE;
    dfu_start_msg.info.img_info.crc16 = 0x2fDf; 
    dfu_start_msg.info.img_info.major_ver = 1;
    dfu_start_msg.info.img_info.minor_ver = 7;
    dfu_start_msg.info.img_info.gitSHA = 0xdeadd00d;
    memcpy(evt.buf, &dfu_start_msg, sizeof(bcmp_dfu_start_t));

    bm_dfu_test_set_dfu_event_and_run_sm(evt);
    EXPECT_EQ(fake_bcmp_tx_func_fake.arg0_history[0], BCMP_DFU_ACK);
    EXPECT_EQ(fake_bcmp_tx_func_fake.arg0_val, BCMP_DFU_PAYLOAD_REQ);
    EXPECT_EQ(getCurrentStateEnum(*ctx), BM_DFU_STATE_CLIENT_RECEIVING);

    bm_dfu_test_set_dfu_event_and_run_sm(evt); // Get a DFU_EVENT_RECEIVED_UPDATE_REQUEST in BM_DFU_STATE_CLIENT_RECEIVING state
    EXPECT_EQ(fake_bcmp_tx_func_fake.arg0_history[0], BCMP_DFU_ACK);
    EXPECT_EQ(fake_bcmp_tx_func_fake.arg0_val, BCMP_DFU_PAYLOAD_REQ);
    EXPECT_EQ(getCurrentStateEnum(*ctx), BM_DFU_STATE_CLIENT_RECEIVING);
}


TEST_F(BcmpDfuTest, hostGolden) {
    bm_dfu_test_set_client_fa(&fa);

    // INIT SUCCESS
    bm_dfu_init(fake_bcmp_tx_func, testPartition);
    libSmContext_t* ctx = bm_dfu_test_get_sm_ctx();
    bm_dfu_event_t evt = {
        .type = DFU_EVENT_INIT_SUCCESS,
        .buf = NULL,
        .len = 0,
    };
    bm_dfu_test_set_dfu_event_and_run_sm(evt);
    EXPECT_EQ(getCurrentStateEnum(*ctx), BM_DFU_STATE_IDLE);

    // HOST REQUEST
    evt.type = DFU_EVENT_BEGIN_HOST;
    evt.buf = (uint8_t*)malloc(sizeof(dfu_host_start_event_t));
    evt.len = sizeof(dfu_host_start_event_t);
    dfu_host_start_event_t dfu_start_msg;
    dfu_start_msg.start.header.frame_type = BCMP_DFU_START;
    dfu_start_msg.start.info.addresses.src_node_id = 0xdeadbeefbeeffeed;
    dfu_start_msg.start.info.addresses.dst_node_id = 0xbeefbeefdaadbaad;
    dfu_start_msg.start.info.img_info.image_size = IMAGE_SIZE;
    dfu_start_msg.start.info.img_info.chunk_size = CHUNK_SIZE;
    dfu_start_msg.start.info.img_info.crc16 = 0x2fDf; 
    dfu_start_msg.start.info.img_info.major_ver = 1;
    dfu_start_msg.start.info.img_info.minor_ver = 7;
    dfu_start_msg.timeoutMs = 30000;
    dfu_start_msg.finish_cb = NULL;
    dfu_start_msg.start.info.img_info.gitSHA = 0xdeadd00d;
    memcpy(evt.buf, &dfu_start_msg, sizeof(dfu_start_msg));

    bm_dfu_test_set_dfu_event_and_run_sm(evt);
    EXPECT_EQ(getCurrentStateEnum(*ctx), BM_DFU_STATE_HOST_REQ_UPDATE);
    EXPECT_EQ(fake_bcmp_tx_func_fake.arg0_val, BCMP_DFU_START);

    // HOST UPDATE
    evt.type = DFU_EVENT_ACK_RECEIVED;
    evt.buf = (uint8_t*)malloc(sizeof(bcmp_dfu_ack_t));
    evt.len = sizeof(bcmp_dfu_ack_t);
    bcmp_dfu_ack_t dfu_ack_msg;
    dfu_ack_msg.header.frame_type = BCMP_DFU_ACK;
    dfu_ack_msg.ack.addresses.dst_node_id = 0xdeadbeefbeeffeed;
    dfu_ack_msg.ack.addresses.src_node_id = 0xbeefbeefdaadbaad;
    dfu_ack_msg.ack.err_code = BM_DFU_ERR_NONE;
    dfu_ack_msg.ack.success = 1;
    memcpy(evt.buf, &dfu_ack_msg, sizeof(bcmp_dfu_ack_t));
    bm_dfu_test_set_dfu_event_and_run_sm(evt);
    EXPECT_EQ(getCurrentStateEnum(*ctx), BM_DFU_STATE_HOST_UPDATE);

    // CHUNK REQUEST
    evt.type = DFU_EVENT_CHUNK_REQUEST;
    evt.buf = (uint8_t*)malloc(sizeof(bcmp_dfu_payload_req_t));
    evt.len = sizeof(bcmp_dfu_payload_req_t);
    bcmp_dfu_payload_req_t dfu_payload_req_msg;
    dfu_payload_req_msg.header.frame_type = BCMP_DFU_PAYLOAD_REQ;
    dfu_payload_req_msg.chunk_req.addresses.dst_node_id = 0xdeadbeefbeeffeed;
    dfu_payload_req_msg.chunk_req.addresses.src_node_id = 0xbeefbeefdaadbaad;
    dfu_payload_req_msg.chunk_req.seq_num  = 0;
    memcpy(evt.buf, &dfu_payload_req_msg, sizeof(dfu_payload_req_msg));
    bm_dfu_test_set_dfu_event_and_run_sm(evt);
    EXPECT_EQ(getCurrentStateEnum(*ctx), BM_DFU_STATE_HOST_UPDATE);
    EXPECT_EQ(fake_bcmp_tx_func_fake.arg0_val, BCMP_DFU_PAYLOAD);

    // REBOOT REQUEST
    evt.type = DFU_EVENT_REBOOT_REQUEST;
    evt.buf = (uint8_t*)malloc(sizeof(bcmp_dfu_reboot_req_t));
    evt.len = sizeof(bcmp_dfu_reboot_req_t);
    bcmp_dfu_reboot_req_t dfu_reboot_req_msg;
    dfu_reboot_req_msg.header.frame_type = BCMP_REBOOT_REQUEST;
    dfu_reboot_req_msg.addr.dst_node_id = 0xdeadbeefbeeffeed;
    dfu_reboot_req_msg.addr.src_node_id = 0xbeefbeefdaadbaad;
    memcpy(evt.buf, &dfu_reboot_req_msg, sizeof(dfu_reboot_req_msg));
    bm_dfu_test_set_dfu_event_and_run_sm(evt);
    EXPECT_EQ(getCurrentStateEnum(*ctx), BM_DFU_STATE_HOST_UPDATE);
    EXPECT_EQ(fake_bcmp_tx_func_fake.arg0_val, BCMP_DFU_REBOOT);

    // REBOOT COMPLETE
    evt.type = DFU_EVENT_BOOT_COMPLETE;
    evt.buf = (uint8_t*)malloc(sizeof(bcmp_dfu_boot_complete_t));
    evt.len = sizeof(bcmp_dfu_boot_complete_t);
    bcmp_dfu_boot_complete_t dfu_reboot_done_msg;
    dfu_reboot_done_msg.header.frame_type = BCMP_REBOOT_REQUEST;
    dfu_reboot_done_msg.addr.dst_node_id = 0xdeadbeefbeeffeed;
    dfu_reboot_done_msg.addr.src_node_id = 0xbeefbeefdaadbaad;
    memcpy(evt.buf, &dfu_reboot_done_msg, sizeof(dfu_reboot_done_msg));
    bm_dfu_test_set_dfu_event_and_run_sm(evt);
    EXPECT_EQ(getCurrentStateEnum(*ctx), BM_DFU_STATE_HOST_UPDATE);
    EXPECT_EQ(fake_bcmp_tx_func_fake.arg0_val, BCMP_DFU_END);

    // DFU EVENT 
    evt.type = DFU_EVENT_UPDATE_END;
    evt.buf = (uint8_t*)malloc(sizeof(bcmp_dfu_end_t));
    evt.len = sizeof(bcmp_dfu_end_t);
    bcmp_dfu_end_t dfu_end_msg;
    dfu_end_msg.header.frame_type = BCMP_DFU_END;
    dfu_end_msg.result.addresses.dst_node_id = 0xdeadbeefbeeffeed;
    dfu_end_msg.result.addresses.src_node_id = 0xbeefbeefdaadbaad;
    dfu_end_msg.result.err_code = BM_DFU_ERR_NONE;
    dfu_end_msg.result.success = 1;
    memcpy(evt.buf, &dfu_end_msg, sizeof(dfu_end_msg));
    bm_dfu_test_set_dfu_event_and_run_sm(evt);
    EXPECT_EQ(getCurrentStateEnum(*ctx), BM_DFU_STATE_IDLE);
    
}

TEST_F(BcmpDfuTest, HostReqUpdateFail){
    bm_dfu_test_set_client_fa(&fa);

    // INIT SUCCESS
    bm_dfu_init(fake_bcmp_tx_func, testPartition);
    libSmContext_t* ctx = bm_dfu_test_get_sm_ctx();
    bm_dfu_event_t evt = {
        .type = DFU_EVENT_INIT_SUCCESS,
        .buf = NULL,
        .len = 0,
    };
    bm_dfu_test_set_dfu_event_and_run_sm(evt);
    EXPECT_EQ(getCurrentStateEnum(*ctx), BM_DFU_STATE_IDLE);

    // HOST REQUEST
    evt.type = DFU_EVENT_BEGIN_HOST;
    evt.buf = (uint8_t*)malloc(sizeof(dfu_host_start_event_t));
    evt.len = sizeof(dfu_host_start_event_t);
    dfu_host_start_event_t dfu_start_msg;
    dfu_start_msg.start.header.frame_type = BCMP_DFU_START;
    dfu_start_msg.start.info.addresses.src_node_id = 0xdeadbeefbeeffeed;
    dfu_start_msg.start.info.addresses.dst_node_id = 0xbeefbeefdaadbaad;
    dfu_start_msg.start.info.img_info.image_size = IMAGE_SIZE;
    dfu_start_msg.start.info.img_info.chunk_size = CHUNK_SIZE;
    dfu_start_msg.start.info.img_info.crc16 = 0x2fDf; 
    dfu_start_msg.start.info.img_info.major_ver = 1;
    dfu_start_msg.start.info.img_info.minor_ver = 7;
    dfu_start_msg.timeoutMs = 30000;
    dfu_start_msg.finish_cb = NULL;
    dfu_start_msg.start.info.img_info.gitSHA = 0xdeadd00d;
    memcpy(evt.buf, &dfu_start_msg, sizeof(dfu_start_msg));

    bm_dfu_test_set_dfu_event_and_run_sm(evt);
    EXPECT_EQ(getCurrentStateEnum(*ctx), BM_DFU_STATE_HOST_REQ_UPDATE);
    EXPECT_EQ(fake_bcmp_tx_func_fake.arg0_val, BCMP_DFU_START);

    evt.type = DFU_EVENT_ACK_TIMEOUT;
    evt.buf = NULL;
    evt.len = 0;
    bm_dfu_test_set_dfu_event_and_run_sm(evt);
    EXPECT_EQ(getCurrentStateEnum(*ctx), BM_DFU_STATE_HOST_REQ_UPDATE); // retry 1
    bm_dfu_test_set_dfu_event_and_run_sm(evt);
    EXPECT_EQ(getCurrentStateEnum(*ctx), BM_DFU_STATE_ERROR); 
    evt.type = DFU_EVENT_NONE;
    bm_dfu_test_set_dfu_event_and_run_sm(evt);
    EXPECT_EQ(getCurrentStateEnum(*ctx), BM_DFU_STATE_IDLE); 

    // HOST REQUEST
    evt.type = DFU_EVENT_BEGIN_HOST;
    evt.buf = (uint8_t*)malloc(sizeof(dfu_host_start_event_t));
    evt.len = sizeof(dfu_host_start_event_t);
    memcpy(evt.buf, &dfu_start_msg, sizeof(dfu_host_start_event_t));

    bm_dfu_test_set_dfu_event_and_run_sm(evt);
    EXPECT_EQ(getCurrentStateEnum(*ctx), BM_DFU_STATE_HOST_REQ_UPDATE);
    EXPECT_EQ(fake_bcmp_tx_func_fake.arg0_val, BCMP_DFU_START);

    // ABORT
    evt.type = DFU_EVENT_ABORT;
    evt.buf = NULL;
    evt.len = 0;
    bm_dfu_test_set_dfu_event_and_run_sm(evt);
    EXPECT_EQ(getCurrentStateEnum(*ctx), BM_DFU_STATE_ERROR); 
    evt.type = DFU_EVENT_NONE;
    bm_dfu_test_set_dfu_event_and_run_sm(evt);
    EXPECT_EQ(getCurrentStateEnum(*ctx), BM_DFU_STATE_IDLE); 
}

TEST_F(BcmpDfuTest, HostUpdateFail){
    bm_dfu_test_set_client_fa(&fa);

    // INIT SUCCESS
    bm_dfu_init(fake_bcmp_tx_func, testPartition);
    libSmContext_t* ctx = bm_dfu_test_get_sm_ctx();
    bm_dfu_event_t evt = {
        .type = DFU_EVENT_INIT_SUCCESS,
        .buf = NULL,
        .len = 0,
    };
    bm_dfu_test_set_dfu_event_and_run_sm(evt);
    EXPECT_EQ(getCurrentStateEnum(*ctx), BM_DFU_STATE_IDLE);

    // HOST REQUEST
    evt.type = DFU_EVENT_BEGIN_HOST;
    evt.buf = (uint8_t*)malloc(sizeof(dfu_host_start_event_t));
    evt.len = sizeof(dfu_host_start_event_t);
    dfu_host_start_event_t dfu_start_msg;
    dfu_start_msg.start.header.frame_type = BCMP_DFU_START;
    dfu_start_msg.start.info.addresses.src_node_id = 0xdeadbeefbeeffeed;
    dfu_start_msg.start.info.addresses.dst_node_id = 0xbeefbeefdaadbaad;
    dfu_start_msg.start.info.img_info.image_size = IMAGE_SIZE;
    dfu_start_msg.start.info.img_info.chunk_size = CHUNK_SIZE;
    dfu_start_msg.start.info.img_info.crc16 = 0x2fDf; 
    dfu_start_msg.start.info.img_info.major_ver = 1;
    dfu_start_msg.start.info.img_info.minor_ver = 7;
    dfu_start_msg.timeoutMs = 30000;
    dfu_start_msg.finish_cb = NULL;
    dfu_start_msg.start.info.img_info.gitSHA = 0xdeadd00d;
    memcpy(evt.buf, &dfu_start_msg, sizeof(dfu_start_msg));

    bm_dfu_test_set_dfu_event_and_run_sm(evt);
    EXPECT_EQ(getCurrentStateEnum(*ctx), BM_DFU_STATE_HOST_REQ_UPDATE);
    EXPECT_EQ(fake_bcmp_tx_func_fake.arg0_val, BCMP_DFU_START);

    // HOST UPDATE
    evt.type = DFU_EVENT_ACK_RECEIVED;
    evt.buf = (uint8_t*)malloc(sizeof(bcmp_dfu_ack_t));
    evt.len = sizeof(bcmp_dfu_ack_t);
    bcmp_dfu_ack_t dfu_ack_msg;
    dfu_ack_msg.header.frame_type = BCMP_DFU_ACK;
    dfu_ack_msg.ack.addresses.dst_node_id = 0xdeadbeefbeeffeed;
    dfu_ack_msg.ack.addresses.src_node_id = 0xbeefbeefdaadbaad;
    dfu_ack_msg.ack.err_code = BM_DFU_ERR_NONE;
    dfu_ack_msg.ack.success = 1;
    memcpy(evt.buf, &dfu_ack_msg, sizeof(bcmp_dfu_ack_t));
    bm_dfu_test_set_dfu_event_and_run_sm(evt);
    EXPECT_EQ(getCurrentStateEnum(*ctx), BM_DFU_STATE_HOST_UPDATE);

    // ABORT
    evt.type = DFU_EVENT_ABORT;
    evt.buf = NULL;
    evt.len = 0;
    bm_dfu_test_set_dfu_event_and_run_sm(evt);
    EXPECT_EQ(getCurrentStateEnum(*ctx), BM_DFU_STATE_ERROR); 
    evt.type = DFU_EVENT_NONE;
    bm_dfu_test_set_dfu_event_and_run_sm(evt);
    EXPECT_EQ(getCurrentStateEnum(*ctx), BM_DFU_STATE_IDLE); 

   // HOST REQUEST
    evt.type = DFU_EVENT_BEGIN_HOST;
    evt.buf = (uint8_t*)malloc(sizeof(dfu_host_start_event_t));
    evt.len = sizeof(dfu_host_start_event_t);
    memcpy(evt.buf, &dfu_start_msg, sizeof(dfu_start_msg));

    bm_dfu_test_set_dfu_event_and_run_sm(evt);
    EXPECT_EQ(getCurrentStateEnum(*ctx), BM_DFU_STATE_HOST_REQ_UPDATE);
    EXPECT_EQ(fake_bcmp_tx_func_fake.arg0_val, BCMP_DFU_START);

    // HOST UPDATE
    evt.type = DFU_EVENT_ACK_RECEIVED;
    evt.buf = (uint8_t*)malloc(sizeof(bcmp_dfu_ack_t));
    evt.len = sizeof(bcmp_dfu_ack_t);
    memcpy(evt.buf, &dfu_ack_msg, sizeof(bcmp_dfu_ack_t));
    bm_dfu_test_set_dfu_event_and_run_sm(evt);
    EXPECT_EQ(getCurrentStateEnum(*ctx), BM_DFU_STATE_HOST_UPDATE);
}

TEST_F(BcmpDfuTest, ClientRecvFail){
    bm_dfu_test_set_client_fa(&fa);

    // INIT SUCCESS
    bm_dfu_init(fake_bcmp_tx_func, testPartition);
    libSmContext_t* ctx = bm_dfu_test_get_sm_ctx();
    bm_dfu_event_t evt = {
        .type = DFU_EVENT_INIT_SUCCESS,
        .buf = NULL,
        .len = 0,
    };
    bm_dfu_test_set_dfu_event_and_run_sm(evt);
    EXPECT_EQ(getCurrentStateEnum(*ctx), BM_DFU_STATE_IDLE);

    // DFU REQUEST
    evt.type = DFU_EVENT_RECEIVED_UPDATE_REQUEST;
    evt.buf = (uint8_t*)malloc(sizeof(bcmp_dfu_start_t));
    evt.len = sizeof(bcmp_dfu_start_t);
    bcmp_dfu_start_t dfu_start_msg;
    dfu_start_msg.header.frame_type = BCMP_DFU_START;
    dfu_start_msg.info.addresses.src_node_id = 0xbeefbeefdaadbaad;
    dfu_start_msg.info.addresses.dst_node_id = 0xdeadbeefbeeffeed;
    dfu_start_msg.info.img_info.image_size = IMAGE_SIZE;
    dfu_start_msg.info.img_info.chunk_size = CHUNK_SIZE;
    dfu_start_msg.info.img_info.crc16 = 0x2fDf; 
    dfu_start_msg.info.img_info.major_ver = 1;
    dfu_start_msg.info.img_info.minor_ver = 7;
    dfu_start_msg.info.img_info.gitSHA = 0xdeadd00d;
    memcpy(evt.buf, &dfu_start_msg, sizeof(bcmp_dfu_start_t));

    bm_dfu_test_set_dfu_event_and_run_sm(evt);
    EXPECT_EQ(fake_bcmp_tx_func_fake.arg0_history[0], BCMP_DFU_ACK);
    EXPECT_EQ(fake_bcmp_tx_func_fake.arg0_val, BCMP_DFU_PAYLOAD_REQ);
    EXPECT_EQ(getCurrentStateEnum(*ctx), BM_DFU_STATE_CLIENT_RECEIVING);

    evt.type = DFU_EVENT_CHUNK_TIMEOUT;
    evt.buf = NULL;
    evt.len = 0;
    bm_dfu_test_set_dfu_event_and_run_sm(evt); // retry 1
    EXPECT_EQ(getCurrentStateEnum(*ctx), BM_DFU_STATE_CLIENT_RECEIVING);
    bm_dfu_test_set_dfu_event_and_run_sm(evt); // retry 2
    EXPECT_EQ(getCurrentStateEnum(*ctx), BM_DFU_STATE_CLIENT_RECEIVING);
    bm_dfu_test_set_dfu_event_and_run_sm(evt); // retry 3
    EXPECT_EQ(getCurrentStateEnum(*ctx), BM_DFU_STATE_CLIENT_RECEIVING);
    bm_dfu_test_set_dfu_event_and_run_sm(evt); // retry 4
    EXPECT_EQ(getCurrentStateEnum(*ctx), BM_DFU_STATE_CLIENT_RECEIVING);
    bm_dfu_test_set_dfu_event_and_run_sm(evt); // retry 5
    EXPECT_EQ(getCurrentStateEnum(*ctx), BM_DFU_STATE_ERROR);
    evt.type = DFU_EVENT_NONE;
    bm_dfu_test_set_dfu_event_and_run_sm(evt);
    EXPECT_EQ(getCurrentStateEnum(*ctx), BM_DFU_STATE_IDLE); 
}

TEST_F(BcmpDfuTest, ClientValidateFail){
    bm_dfu_test_set_client_fa(&fa);

    // INIT SUCCESS
    bm_dfu_init(fake_bcmp_tx_func, testPartition);
    libSmContext_t* ctx = bm_dfu_test_get_sm_ctx();
    bm_dfu_event_t evt = {
        .type = DFU_EVENT_INIT_SUCCESS,
        .buf = NULL,
        .len = 0,
    };
    bm_dfu_test_set_dfu_event_and_run_sm(evt);
    EXPECT_EQ(getCurrentStateEnum(*ctx), BM_DFU_STATE_IDLE);

    // DFU REQUEST
    evt.type = DFU_EVENT_RECEIVED_UPDATE_REQUEST;
    evt.buf = (uint8_t*)malloc(sizeof(bcmp_dfu_start_t));
    evt.len = sizeof(bcmp_dfu_start_t);
    bcmp_dfu_start_t dfu_start_msg;
    dfu_start_msg.header.frame_type = BCMP_DFU_START;
    dfu_start_msg.info.addresses.src_node_id = 0xbeefbeefdaadbaad;
    dfu_start_msg.info.addresses.dst_node_id = 0xdeadbeefbeeffeed;
    dfu_start_msg.info.img_info.image_size = IMAGE_SIZE;
    dfu_start_msg.info.img_info.chunk_size = CHUNK_SIZE;
    dfu_start_msg.info.img_info.crc16 = 0xDEAD; // bad CRC
    dfu_start_msg.info.img_info.major_ver = 1;
    dfu_start_msg.info.img_info.minor_ver = 7;
    dfu_start_msg.info.img_info.gitSHA = 0xdeadd00d;
    memcpy(evt.buf, &dfu_start_msg, sizeof(bcmp_dfu_start_t));

    bm_dfu_test_set_dfu_event_and_run_sm(evt);
    EXPECT_EQ(fake_bcmp_tx_func_fake.arg0_history[0], BCMP_DFU_ACK);
    EXPECT_EQ(fake_bcmp_tx_func_fake.arg0_val, BCMP_DFU_PAYLOAD_REQ);
    EXPECT_EQ(getCurrentStateEnum(*ctx), BM_DFU_STATE_CLIENT_RECEIVING);

    // Chunk
    evt.type = DFU_EVENT_IMAGE_CHUNK;
    evt.buf = (uint8_t*)malloc(sizeof(bcmp_dfu_payload_t) + CHUNK_SIZE);
    evt.len = sizeof(bcmp_dfu_payload_t) + CHUNK_SIZE;
    bcmp_dfu_payload_t dfu_payload_msg;
    dfu_payload_msg.header.frame_type = BCMP_DFU_PAYLOAD;
    dfu_payload_msg.chunk.addresses.src_node_id = 0xbeefbeefdaadbaad;
    dfu_payload_msg.chunk.addresses.dst_node_id = 0xdeadbeefbeeffeed;
    dfu_payload_msg.chunk.payload_length = CHUNK_SIZE;
    memcpy(evt.buf, &dfu_payload_msg, sizeof(dfu_payload_msg));
    memset(evt.buf+sizeof(dfu_payload_msg),0xa5,CHUNK_SIZE);
    bm_dfu_test_set_dfu_event_and_run_sm(evt); // 512
    EXPECT_EQ(fake_bcmp_tx_func_fake.arg0_val, BCMP_DFU_PAYLOAD_REQ);
    EXPECT_EQ(getCurrentStateEnum(*ctx), BM_DFU_STATE_CLIENT_RECEIVING); 
    bm_dfu_test_set_dfu_event_and_run_sm(evt); // 1024
    EXPECT_EQ(fake_bcmp_tx_func_fake.arg0_val, BCMP_DFU_PAYLOAD_REQ);
    EXPECT_EQ(getCurrentStateEnum(*ctx), BM_DFU_STATE_CLIENT_RECEIVING);
    bm_dfu_test_set_dfu_event_and_run_sm(evt); // 1536
    EXPECT_EQ(fake_bcmp_tx_func_fake.arg0_val, BCMP_DFU_PAYLOAD_REQ);
    EXPECT_EQ(getCurrentStateEnum(*ctx), BM_DFU_STATE_CLIENT_RECEIVING);
    bm_dfu_test_set_dfu_event_and_run_sm(evt); // 2048
    EXPECT_EQ(getCurrentStateEnum(*ctx), BM_DFU_STATE_CLIENT_VALIDATING);

   // Validating
    evt.type = DFU_EVENT_NONE;
    evt.buf = NULL;
    evt.len = 0;
    bm_dfu_test_set_dfu_event_and_run_sm(evt); 
    EXPECT_EQ(getCurrentStateEnum(*ctx), BM_DFU_STATE_ERROR); // FAILED to validate CRC
    evt.type = DFU_EVENT_NONE;
    EXPECT_EQ(bm_dfu_get_error(),BM_DFU_ERR_BAD_CRC); 
    bm_dfu_test_set_dfu_event_and_run_sm(evt);
    EXPECT_EQ(getCurrentStateEnum(*ctx), BM_DFU_STATE_IDLE); 

    // DFU REQUEST
    evt.type = DFU_EVENT_RECEIVED_UPDATE_REQUEST;
    evt.buf = (uint8_t*)malloc(sizeof(bcmp_dfu_start_t));
    evt.len = sizeof(bcmp_dfu_start_t);
    memcpy(evt.buf, &dfu_start_msg, sizeof(bcmp_dfu_start_t));

    bm_dfu_test_set_dfu_event_and_run_sm(evt);
    EXPECT_EQ(fake_bcmp_tx_func_fake.arg0_history[0], BCMP_DFU_ACK);
    EXPECT_EQ(fake_bcmp_tx_func_fake.arg0_val, BCMP_DFU_PAYLOAD_REQ);
    EXPECT_EQ(getCurrentStateEnum(*ctx), BM_DFU_STATE_CLIENT_RECEIVING);

    // Chunk
    evt.type = DFU_EVENT_IMAGE_CHUNK;
    evt.buf = (uint8_t*)malloc(sizeof(bcmp_dfu_payload_t) + CHUNK_SIZE);
    evt.len = sizeof(bcmp_dfu_payload_t) + CHUNK_SIZE;
    memcpy(evt.buf, &dfu_payload_msg, sizeof(dfu_payload_msg));
    memset(evt.buf+sizeof(dfu_payload_msg),0xa5,CHUNK_SIZE);
    bm_dfu_test_set_dfu_event_and_run_sm(evt); // 512
    EXPECT_EQ(fake_bcmp_tx_func_fake.arg0_val, BCMP_DFU_PAYLOAD_REQ);
    EXPECT_EQ(getCurrentStateEnum(*ctx), BM_DFU_STATE_CLIENT_RECEIVING); 
    bm_dfu_test_set_dfu_event_and_run_sm(evt); // 1024
    EXPECT_EQ(fake_bcmp_tx_func_fake.arg0_val, BCMP_DFU_PAYLOAD_REQ);
    EXPECT_EQ(getCurrentStateEnum(*ctx), BM_DFU_STATE_CLIENT_RECEIVING);
    bm_dfu_test_set_dfu_event_and_run_sm(evt); // 1536
    EXPECT_EQ(fake_bcmp_tx_func_fake.arg0_val, BCMP_DFU_PAYLOAD_REQ);
    EXPECT_EQ(getCurrentStateEnum(*ctx), BM_DFU_STATE_CLIENT_RECEIVING);
    dfu_payload_msg.chunk.payload_length = 1;
    memcpy(evt.buf, &dfu_payload_msg, sizeof(dfu_payload_msg));
    bm_dfu_test_set_dfu_event_and_run_sm(evt); // 1537
    EXPECT_EQ(getCurrentStateEnum(*ctx), BM_DFU_STATE_CLIENT_VALIDATING);

   // Validating
    evt.type = DFU_EVENT_NONE;
    evt.buf = NULL;
    evt.len = 0;
    bm_dfu_test_set_dfu_event_and_run_sm(evt); 
    EXPECT_EQ(getCurrentStateEnum(*ctx), BM_DFU_STATE_ERROR); // Image offest does not equal image length.
    evt.type = DFU_EVENT_NONE;
    EXPECT_EQ(bm_dfu_get_error(),BM_DFU_ERR_MISMATCH_LEN); 
    bm_dfu_test_set_dfu_event_and_run_sm(evt);
    EXPECT_EQ(getCurrentStateEnum(*ctx), BM_DFU_STATE_IDLE); 
}


TEST_F(BcmpDfuTest, ChunksTooBig){
    bm_dfu_test_set_client_fa(&fa);

    // INIT SUCCESS
    bm_dfu_init(fake_bcmp_tx_func, testPartition);
    libSmContext_t* ctx = bm_dfu_test_get_sm_ctx();
    bm_dfu_event_t evt = {
        .type = DFU_EVENT_INIT_SUCCESS,
        .buf = NULL,
        .len = 0,
    };
    bm_dfu_test_set_dfu_event_and_run_sm(evt);
    EXPECT_EQ(getCurrentStateEnum(*ctx), BM_DFU_STATE_IDLE);

    // DFU REQUEST
    evt.type = DFU_EVENT_RECEIVED_UPDATE_REQUEST;
    evt.buf = (uint8_t*)malloc(sizeof(bcmp_dfu_start_t));
    evt.len = sizeof(bcmp_dfu_start_t);
    bcmp_dfu_start_t dfu_start_msg;
    dfu_start_msg.header.frame_type = BCMP_DFU_START;
    dfu_start_msg.info.addresses.src_node_id = 0xbeefbeefdaadbaad;
    dfu_start_msg.info.addresses.dst_node_id = 0xdeadbeefbeeffeed;
    dfu_start_msg.info.img_info.image_size = IMAGE_SIZE;
    dfu_start_msg.info.img_info.chunk_size = CHUNK_SIZE * 100; // Chunk way too big
    dfu_start_msg.info.img_info.crc16 = 0xDEAD; // bad CRC
    dfu_start_msg.info.img_info.major_ver = 1;
    dfu_start_msg.info.img_info.minor_ver = 7;
    dfu_start_msg.info.img_info.gitSHA = 0xdeadd00d;
    memcpy(evt.buf, &dfu_start_msg, sizeof(bcmp_dfu_start_t));

    bm_dfu_test_set_dfu_event_and_run_sm(evt);
    EXPECT_EQ(getCurrentStateEnum(*ctx), BM_DFU_STATE_ERROR); // FAILED to validate CRC
    EXPECT_EQ(bm_dfu_get_error(),BM_DFU_ERR_CHUNK_SIZE); 
}

TEST_F(BcmpDfuTest, ClientRebootReqFail){
    bm_dfu_test_set_client_fa(&fa);

    // INIT SUCCESS
    bm_dfu_init(fake_bcmp_tx_func, testPartition);
    libSmContext_t* ctx = bm_dfu_test_get_sm_ctx();
    bm_dfu_event_t evt = {
        .type = DFU_EVENT_INIT_SUCCESS,
        .buf = NULL,
        .len = 0,
    };
    bm_dfu_test_set_dfu_event_and_run_sm(evt);
    EXPECT_EQ(getCurrentStateEnum(*ctx), BM_DFU_STATE_IDLE);

    // DFU REQUEST
    evt.type = DFU_EVENT_RECEIVED_UPDATE_REQUEST;
    evt.buf = (uint8_t*)malloc(sizeof(bcmp_dfu_start_t));
    evt.len = sizeof(bcmp_dfu_start_t);
    bcmp_dfu_start_t dfu_start_msg;
    dfu_start_msg.header.frame_type = BCMP_DFU_START;
    dfu_start_msg.info.addresses.src_node_id = 0xbeefbeefdaadbaad;
    dfu_start_msg.info.addresses.dst_node_id = 0xdeadbeefbeeffeed;
    dfu_start_msg.info.img_info.image_size = IMAGE_SIZE;
    dfu_start_msg.info.img_info.chunk_size = CHUNK_SIZE;
    dfu_start_msg.info.img_info.crc16 = 0x2fDf; 
    dfu_start_msg.info.img_info.major_ver = 1;
    dfu_start_msg.info.img_info.minor_ver = 7;
    dfu_start_msg.info.img_info.gitSHA = 0xdeadd00d;
    memcpy(evt.buf, &dfu_start_msg, sizeof(bcmp_dfu_start_t));

    bm_dfu_test_set_dfu_event_and_run_sm(evt);
    EXPECT_EQ(fake_bcmp_tx_func_fake.arg0_history[0], BCMP_DFU_ACK);
    EXPECT_EQ(fake_bcmp_tx_func_fake.arg0_val, BCMP_DFU_PAYLOAD_REQ);
    EXPECT_EQ(getCurrentStateEnum(*ctx), BM_DFU_STATE_CLIENT_RECEIVING);

    // Chunk
    evt.type = DFU_EVENT_IMAGE_CHUNK;
    evt.buf = (uint8_t*)malloc(sizeof(bcmp_dfu_payload_t) + CHUNK_SIZE);
    evt.len = sizeof(bcmp_dfu_payload_t) + CHUNK_SIZE;
    bcmp_dfu_payload_t dfu_payload_msg;
    dfu_payload_msg.header.frame_type = BCMP_DFU_PAYLOAD;
    dfu_payload_msg.chunk.addresses.src_node_id = 0xbeefbeefdaadbaad;
    dfu_payload_msg.chunk.addresses.dst_node_id = 0xdeadbeefbeeffeed;
    dfu_payload_msg.chunk.payload_length = CHUNK_SIZE;
    memcpy(evt.buf, &dfu_payload_msg, sizeof(dfu_payload_msg));
    memset(evt.buf+sizeof(dfu_payload_msg),0xa5,CHUNK_SIZE);
    bm_dfu_test_set_dfu_event_and_run_sm(evt); // 512
    EXPECT_EQ(fake_bcmp_tx_func_fake.arg0_val, BCMP_DFU_PAYLOAD_REQ);
    EXPECT_EQ(getCurrentStateEnum(*ctx), BM_DFU_STATE_CLIENT_RECEIVING); 
    bm_dfu_test_set_dfu_event_and_run_sm(evt); // 1024
    EXPECT_EQ(fake_bcmp_tx_func_fake.arg0_val, BCMP_DFU_PAYLOAD_REQ);
    EXPECT_EQ(getCurrentStateEnum(*ctx), BM_DFU_STATE_CLIENT_RECEIVING);
    bm_dfu_test_set_dfu_event_and_run_sm(evt); // 1536
    EXPECT_EQ(fake_bcmp_tx_func_fake.arg0_val, BCMP_DFU_PAYLOAD_REQ);
    EXPECT_EQ(getCurrentStateEnum(*ctx), BM_DFU_STATE_CLIENT_RECEIVING);
    bm_dfu_test_set_dfu_event_and_run_sm(evt); // 2048
    EXPECT_EQ(getCurrentStateEnum(*ctx), BM_DFU_STATE_CLIENT_VALIDATING);

    // Validating
    evt.type = DFU_EVENT_NONE;
    evt.buf = NULL;
    evt.len = 0;
    bm_dfu_test_set_dfu_event_and_run_sm(evt); 
    EXPECT_EQ(getCurrentStateEnum(*ctx), BM_DFU_STATE_CLIENT_REBOOT_REQ);
    EXPECT_EQ(fake_bcmp_tx_func_fake.arg0_val, BCMP_DFU_REBOOT_REQ);

    evt.type = DFU_EVENT_CHUNK_TIMEOUT;
    evt.buf = NULL;
    evt.len = 0;
    bm_dfu_test_set_dfu_event_and_run_sm(evt); // retry 1
    EXPECT_EQ(getCurrentStateEnum(*ctx), BM_DFU_STATE_CLIENT_REBOOT_REQ);
    bm_dfu_test_set_dfu_event_and_run_sm(evt); // retry 2
    EXPECT_EQ(getCurrentStateEnum(*ctx), BM_DFU_STATE_CLIENT_REBOOT_REQ);
    bm_dfu_test_set_dfu_event_and_run_sm(evt); // retry 3
    EXPECT_EQ(getCurrentStateEnum(*ctx), BM_DFU_STATE_CLIENT_REBOOT_REQ);
    bm_dfu_test_set_dfu_event_and_run_sm(evt); // retry 4
    EXPECT_EQ(getCurrentStateEnum(*ctx), BM_DFU_STATE_CLIENT_REBOOT_REQ);
    bm_dfu_test_set_dfu_event_and_run_sm(evt); // retry 5
    EXPECT_EQ(getCurrentStateEnum(*ctx), BM_DFU_STATE_ERROR);
    evt.type = DFU_EVENT_NONE;
    bm_dfu_test_set_dfu_event_and_run_sm(evt);
    EXPECT_EQ(getCurrentStateEnum(*ctx), BM_DFU_STATE_IDLE); 
}

TEST_F(BcmpDfuTest, RebootDoneFail) {
    // Set the reboot info.
    client_update_reboot_info.magic = DFU_REBOOT_MAGIC;
    client_update_reboot_info.host_node_id = 0xbeefbeefdaadbaad;
    client_update_reboot_info.major = 0; // Incorrect major
    client_update_reboot_info.minor = 7;
    client_update_reboot_info.gitSHA = 0xbaaddead;

    bm_dfu_test_set_client_fa(&fa);

    // INIT SUCCESS
    bm_dfu_init(fake_bcmp_tx_func, testPartition);
    libSmContext_t* ctx = bm_dfu_test_get_sm_ctx();
    bm_dfu_event_t evt = {
        .type = DFU_EVENT_INIT_SUCCESS,
        .buf = NULL,
        .len = 0,
    };
    bm_dfu_test_set_dfu_event_and_run_sm(evt);
    EXPECT_EQ(getCurrentStateEnum(*ctx), BM_DFU_STATE_CLIENT_REBOOT_DONE);
    EXPECT_EQ(resetSystem_fake.arg0_val, RESET_REASON_UPDATE_FAILED);

    // Set the reboot info.
    RESET_FAKE(resetSystem);
    client_update_reboot_info.magic = DFU_REBOOT_MAGIC;
    client_update_reboot_info.host_node_id = 0xbeefbeefdaadbaad;
    client_update_reboot_info.major = 1; 
    client_update_reboot_info.minor = 7;
    client_update_reboot_info.gitSHA = 0xdeadd00d;
    getGitSHA_fake.return_val = 0xdeadd00d;

    bm_dfu_test_set_client_fa(&fa);

    // INIT SUCCESS
    bm_dfu_init(fake_bcmp_tx_func, testPartition);
    bm_dfu_test_set_dfu_event_and_run_sm(evt);
    EXPECT_EQ(getCurrentStateEnum(*ctx), BM_DFU_STATE_CLIENT_REBOOT_DONE);
    EXPECT_EQ(fake_bcmp_tx_func_fake.arg0_val, BCMP_DFU_BOOT_COMPLETE);
    evt.type = DFU_EVENT_CHUNK_TIMEOUT;
    evt.buf = NULL;
    evt.len = 0;
    bm_dfu_test_set_dfu_event_and_run_sm(evt); // retry 1
    EXPECT_EQ(getCurrentStateEnum(*ctx), BM_DFU_STATE_CLIENT_REBOOT_DONE);
    bm_dfu_test_set_dfu_event_and_run_sm(evt); // retry 2
    EXPECT_EQ(getCurrentStateEnum(*ctx), BM_DFU_STATE_CLIENT_REBOOT_DONE);
    bm_dfu_test_set_dfu_event_and_run_sm(evt); // retry 3
    EXPECT_EQ(getCurrentStateEnum(*ctx), BM_DFU_STATE_CLIENT_REBOOT_DONE);
    bm_dfu_test_set_dfu_event_and_run_sm(evt); // retry 4
    EXPECT_EQ(getCurrentStateEnum(*ctx), BM_DFU_STATE_CLIENT_REBOOT_DONE);
    bm_dfu_test_set_dfu_event_and_run_sm(evt); // retry 5
    EXPECT_EQ(resetSystem_fake.arg0_val, RESET_REASON_UPDATE_FAILED);
}
