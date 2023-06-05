#include <stdio.h>
#include <string.h>
#include "bm_dfu.h"
#include "bm_dfu_host.h"
#include "device_info.h"
#include "external_flash_partitions.h"
#include "FreeRTOS.h"
#include "timer_callback_handler.h"

typedef struct dfu_host_ctx_t {
    QueueHandle_t dfu_event_queue;
    TimerHandle_t ack_timer;
    uint8_t ack_retry_num;
    TimerHandle_t heartbeat_timer;
    bm_dfu_img_info_t img_info;
    uint64_t self_node_id;
    uint64_t client_node_id;
    bcmp_dfu_tx_func_t bcmp_dfu_tx;
    NvmPartition * dfu_partition;
    uint32_t bytes_remaining;
    update_finish_cb_t update_complete_callback;
    TimerHandle_t update_timer;
    uint32_t host_timeout_ms;
} dfu_host_ctx_t;

static constexpr uint32_t FLASH_READ_TIMEOUT_MS = 5 * 1000;

static dfu_host_ctx_t host_ctx;

static void ack_timer_handler(TimerHandle_t tmr);
static void heartbeat_timer_handler(TimerHandle_t tmr);
static void update_timer_handler(TimerHandle_t tmr);

static void bm_dfu_host_req_update();
static void bm_dfu_host_send_reboot();
static void bm_dfu_host_transition_to_error(bm_dfu_err_t err);
static void bm_dfu_host_start_update_timer(uint32_t timeoutMs);

/**
 * @brief ACK Timer Handler function
 *
 * @note Puts ACK Timeout event into DFU Subsystem event queue
 *
 * @param *tmr    Pointer to Timer struct
 * @return none
 */
static void ack_timer_handler(TimerHandle_t tmr) {
    (void) tmr;
    bm_dfu_event_t evt = {DFU_EVENT_ACK_TIMEOUT, NULL,0};

    if(xQueueSend(host_ctx.dfu_event_queue, &evt, 0) != pdTRUE) {
        configASSERT(false);
    }
}

/**
 * @brief  hook to send a DFU heartbeat out on the timer_callback_handler thread
 *
 * @note Sends a heartbeat to the client.
 *
 * @param *arg
 * @return none
 */
static void send_hb_timer_cb(void *arg) {
    (void) arg;
    bm_dfu_send_heartbeat(host_ctx.client_node_id);
}

/**
 * @brief Heartbeat Timer Handler function
 *
 * @note Puts Heartbeat event onto the timer_callback_handler thread
 *
 * @param *tmr    Pointer to Timer struct
 * @return none
 */
static void heartbeat_timer_handler(TimerHandle_t tmr) {
    (void) tmr;
    timer_callback_handler_send_cb(send_hb_timer_cb,NULL,0);
}

/**
 * @brief Update Timer Handler Function
 *
 * @note Aborts DFU if timer fires.
 *
 * @return none
 */
static void update_timer_handler(TimerHandle_t tmr) {
    (void) tmr;
    bm_dfu_event_t evt = {DFU_EVENT_ABORT, NULL,0};

    if(xQueueSend(host_ctx.dfu_event_queue, &evt, 0) != pdTRUE) {
        configASSERT(false);
    }
}

/**
 * @brief Send Request Update to Client
 *
 * @note Stuff Update Request bm_frame with image info and put into BM Serial TX Queue
 *
 * @return none
 */
static void bm_dfu_host_req_update() {
    bcmp_dfu_start_t update_start_req_evt;

    printf("Sending Update to Client\n");

    /* Populate the appropriate event */
    update_start_req_evt.info.img_info = host_ctx.img_info;
    update_start_req_evt.info.addresses.src_node_id = host_ctx.self_node_id;
    update_start_req_evt.info.addresses.dst_node_id = host_ctx.client_node_id;
    update_start_req_evt.header.frame_type = BCMP_DFU_START;
    if(host_ctx.bcmp_dfu_tx(static_cast<bcmp_message_type_t>(update_start_req_evt.header.frame_type), reinterpret_cast<uint8_t *>(&update_start_req_evt), sizeof(update_start_req_evt))){
        printf("Message %d sent \n",update_start_req_evt.header.frame_type);
    } else {
        printf("Failed to send message %d\n",update_start_req_evt.header.frame_type);
    }
}

/**
 * @brief Send Chunk to Client
 *
 * @note Stuff bm_frame with image chunk and put into BM Serial TX Queue
 *
 * @return none
 */
static void bm_dfu_host_send_chunk(bm_dfu_event_chunk_request_t* req) {
    printf("Processing chunk id %" PRIX32 "\n",req->seq_num);
    uint32_t payload_len = (host_ctx.bytes_remaining >= host_ctx.img_info.chunk_size) ? host_ctx.img_info.chunk_size : host_ctx.bytes_remaining;
    uint32_t payload_len_plus_header = sizeof(bcmp_dfu_payload_t) + payload_len;
    uint8_t* buf = static_cast<uint8_t*>(pvPortMalloc(payload_len_plus_header));
    configASSERT(buf);
    bcmp_dfu_payload_t *payload_header = reinterpret_cast<bcmp_dfu_payload_t *>(buf);
    payload_header->header.frame_type = BCMP_DFU_PAYLOAD;
    payload_header->chunk.addresses.src_node_id = host_ctx.self_node_id;
    payload_header->chunk.addresses.dst_node_id = host_ctx.client_node_id;
    payload_header->chunk.payload_length = payload_len;

    uint32_t flash_offset = DFU_IMG_START_OFFSET_BYTES + host_ctx.img_info.image_size - host_ctx.bytes_remaining;
    do {
        if(!host_ctx.dfu_partition->read(flash_offset, payload_header->chunk.payload_buf, payload_len, FLASH_READ_TIMEOUT_MS)){
            printf("Failed to read chunk from flash.\n");
            bm_dfu_host_transition_to_error(BM_DFU_ERR_FLASH_ACCESS);
            break;
        }
        if(host_ctx.bcmp_dfu_tx(static_cast<bcmp_message_type_t>(payload_header->header.frame_type), buf, payload_len_plus_header)){
            host_ctx.bytes_remaining -= payload_len;
            printf("Message %d sent, payload size: %" PRIX32 ", remaining: %" PRIX32 "\n",payload_header->header.frame_type, payload_len, host_ctx.bytes_remaining);
        } else {
            printf("Failed to send message %d\n",payload_header->header.frame_type);
            bm_dfu_host_transition_to_error(BM_DFU_ERR_IMG_CHUNK_ACCESS);
        }
    } while(0);

    vPortFree(buf);
}

/**
 * @brief Send an update reboot to Client
 *
 * @return none
 */
static void bm_dfu_host_send_reboot() {
    bcmp_dfu_reboot_t reboot_msg;
    reboot_msg.addr.src_node_id = host_ctx.self_node_id;
    reboot_msg.addr.dst_node_id = host_ctx.client_node_id;
    reboot_msg.header.frame_type = BCMP_DFU_REBOOT;
    if(host_ctx.bcmp_dfu_tx(static_cast<bcmp_message_type_t>(reboot_msg.header.frame_type), reinterpret_cast<uint8_t*>(&reboot_msg), sizeof(bcmp_dfu_reboot_t))){
        printf("Message %d sent \n",reboot_msg.header.frame_type);
    } else {
        printf("Failed to send message %d\n",reboot_msg.header.frame_type);
    }
}

/**
 * @brief Entry Function for the Request Update State
 *
 * @note The Host sends an update request to the client and starts the ACK timeout timer
 *
 * @return none
 */
void s_host_req_update_entry(void) {
    bm_dfu_event_t curr_evt = bm_dfu_get_current_event();

    /* Check if we even have a buf to inspect */
    if (! curr_evt.buf) {
        return;
    }

    bm_dfu_frame_t *frame = reinterpret_cast<bm_dfu_frame_t *>(curr_evt.buf);
    bm_dfu_event_img_info_t* img_info_evt = reinterpret_cast<bm_dfu_event_img_info_t*>(&(reinterpret_cast<uint8_t *>(frame))[1]);
    host_ctx.img_info = img_info_evt->img_info;
    host_ctx.bytes_remaining = host_ctx.img_info.image_size;
    host_ctx.client_node_id = img_info_evt->addresses.dst_node_id;

    printf("DFU Client Node Id: %" PRIx64 "\n", host_ctx.client_node_id);

    host_ctx.ack_retry_num = 0;
    /* Request Client Firmware Update */
    bm_dfu_host_req_update();

    /* Kickoff ACK timeout */
    configASSERT(xTimerStart(host_ctx.ack_timer, 10));
}

/**
 * @brief Run Function for the Request Update State
 *
 * @note The state is waiting on an ACK from the client to begin the update. Returns to idle state on timeout
 *
 * @return none
 */
void s_host_req_update_run(void)
{
    bm_dfu_event_t curr_evt = bm_dfu_get_current_event();

    if (curr_evt.type == DFU_EVENT_ACK_RECEIVED) {
        /* Stop ACK Timer */
        configASSERT(xTimerStop(host_ctx.ack_timer, 10));
        configASSERT(curr_evt.buf);
        bm_dfu_frame_t *frame = reinterpret_cast<bm_dfu_frame_t *>(curr_evt.buf);
        bm_dfu_event_result_t* result_evt = reinterpret_cast<bm_dfu_event_result_t*>(&(reinterpret_cast<uint8_t *>(frame))[1]);

        if (result_evt->success) {
            bm_dfu_set_pending_state_change(BM_DFU_STATE_HOST_UPDATE);
        } else {
            bm_dfu_host_transition_to_error(static_cast<bm_dfu_err_t>(result_evt->err_code));
        }
    } else if (curr_evt.type == DFU_EVENT_ACK_TIMEOUT) {
        host_ctx.ack_retry_num++;

        /* Wait for ack until max retries is reached */
        if (host_ctx.ack_retry_num >= BM_DFU_MAX_ACK_RETRIES) {
            bm_dfu_host_transition_to_error(BM_DFU_ERR_TIMEOUT);
        } else {
            bm_dfu_host_req_update();
            configASSERT(xTimerStart(host_ctx.ack_timer, 10));
        }
    } else if (curr_evt.type == DFU_EVENT_ABORT) {
        printf("Recieved abort.\n");
        bm_dfu_host_transition_to_error(BM_DFU_ERR_ABORTED);
    }
}

/**
 * @brief Entry Function for the Update State
 *
 * @note The Host starts the host global timeout.
 *
 * @return none
 */
void s_host_update_entry(void) {
    bm_dfu_host_start_update_timer(host_ctx.host_timeout_ms);
}

/**
 * @brief Run Function for the Update State
 *
 * @note Host state that sends chunks of image to Client. Exits on global timeout timeout or abort/end message received from client
 *
 * @return none
 */
void s_host_update_run(void) {
    bm_dfu_frame_t *frame = NULL;
    bm_dfu_event_t curr_evt = bm_dfu_get_current_event();

    /* Check if we even have a buf to inspect */
    if (curr_evt.buf) {
        frame = reinterpret_cast<bm_dfu_frame_t *>(curr_evt.buf);
    }

    if (curr_evt.type == DFU_EVENT_CHUNK_REQUEST) {
        configASSERT(frame);
        bm_dfu_event_chunk_request_t* chunk_req_evt = reinterpret_cast<bm_dfu_event_chunk_request_t*>(&(reinterpret_cast<uint8_t *>(frame))[1]);

        /* Request Next Chunk */
        /* Send Heartbeat to Client */
        configASSERT(xTimerStart(host_ctx.heartbeat_timer, 10));

        /* resend the frame to the client as is */
        bm_dfu_host_send_chunk(chunk_req_evt);

        configASSERT(xTimerStop(host_ctx.heartbeat_timer, 10));
    } else if (curr_evt.type == DFU_EVENT_REBOOT_REQUEST) {
        configASSERT(frame);
        bm_dfu_host_send_reboot();
    } else if (curr_evt.type == DFU_EVENT_BOOT_COMPLETE) {
        configASSERT(frame);
        bm_dfu_update_end(host_ctx.client_node_id, true, BM_DFU_ERR_NONE);
    }
    else if (curr_evt.type == DFU_EVENT_UPDATE_END) {
        configASSERT(xTimerStop(host_ctx.update_timer, 100));
        configASSERT(frame);
        bm_dfu_event_result_t* update_end_evt = reinterpret_cast<bm_dfu_event_result_t*>(&(reinterpret_cast<uint8_t *>(frame))[1]);

        if (update_end_evt->success) {
            printf("Successfully updated Client\n");
        } else {
            printf("Client Update Failed\n");
        }
        if(host_ctx.update_complete_callback) {
            host_ctx.update_complete_callback(update_end_evt->success, static_cast<bm_dfu_err_t>(update_end_evt->err_code), host_ctx.client_node_id);
        }
        bm_dfu_set_pending_state_change(BM_DFU_STATE_IDLE);
    } else if (curr_evt.type == DFU_EVENT_ABORT) {
        printf("Recieved abort.\n");
        bm_dfu_host_transition_to_error(BM_DFU_ERR_ABORTED);
    }
}

void bm_dfu_host_init(bcmp_dfu_tx_func_t bcmp_dfu_tx, NvmPartition * dfu_partition) {
    configASSERT(bcmp_dfu_tx);
    configASSERT(dfu_partition);
    host_ctx.bcmp_dfu_tx = bcmp_dfu_tx;
    host_ctx.dfu_partition = dfu_partition;
    int tmr_id = 0;

    /* Store relevant variables */
    host_ctx.self_node_id = getNodeId();

    /* Get DFU Subsystem Queue */
    host_ctx.dfu_event_queue = bm_dfu_get_event_queue();

    /* Initialize ACK and Heartbeat Timer */
    host_ctx.ack_timer = xTimerCreate("DFU Host Ack", (BM_DFU_HOST_ACK_TIMEOUT_MS / portTICK_RATE_MS),
                                      pdFALSE, (void *) &tmr_id, ack_timer_handler);
    configASSERT(host_ctx.ack_timer);

    host_ctx.heartbeat_timer = xTimerCreate("DFU Host Heartbeat", (BM_DFU_HOST_HEARTBEAT_TIMEOUT_MS / portTICK_RATE_MS),
                                      pdTRUE, (void *) &tmr_id, heartbeat_timer_handler);
    configASSERT(host_ctx.heartbeat_timer);
    host_ctx.update_timer = xTimerCreate("update timer", pdMS_TO_TICKS(BM_DFU_UPDATE_DEFAULT_TIMEOUT_MS),
                                    pdFALSE, (void *) &tmr_id, update_timer_handler);
    configASSERT(host_ctx.update_timer);
}

void bm_dfu_host_set_params(update_finish_cb_t update_complete_callback, uint32_t hostTimeoutMs) {
    host_ctx.update_complete_callback = update_complete_callback;
    host_ctx.host_timeout_ms = hostTimeoutMs;
}

static void bm_dfu_host_start_update_timer(uint32_t timeoutMs) {
    configASSERT(xTimerStop(host_ctx.update_timer, 100));
    configASSERT(xTimerChangePeriod(host_ctx.update_timer, pdMS_TO_TICKS(timeoutMs), 100));
    configASSERT(xTimerStart(host_ctx.update_timer, 100));
}

static void bm_dfu_host_transition_to_error(bm_dfu_err_t err) {
    configASSERT(xTimerStop(host_ctx.update_timer, 100));
    configASSERT(xTimerStop(host_ctx.heartbeat_timer, 10));
    configASSERT(xTimerStop(host_ctx.ack_timer, 10));
    bm_dfu_set_error(err);
    bm_dfu_set_pending_state_change(BM_DFU_STATE_ERROR);
}

bool bm_dfu_host_client_node_valid(uint64_t client_node_id) {
    return host_ctx.client_node_id == client_node_id;
}
