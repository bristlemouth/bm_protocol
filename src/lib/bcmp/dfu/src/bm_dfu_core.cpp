#include <string.h>
#include <stdio.h>

#include "FreeRTOS.h"
#include "semphr.h"

#include "bm_dfu.h"
#include "bm_dfu_client.h"
#include "bm_dfu_host.h"
#include "task_priorities.h"
#include "device_info.h"

typedef struct dfu_core_ctx_t {
    libSmContext_t sm_ctx;
    bm_dfu_event_t current_event;
    bool pending_state_change;
    uint8_t new_state;
    bm_dfu_err_t error;
    uint64_t self_node_id;
    bcmp_dfu_tx_func_t bcmp_dfu_tx;
    NvmPartition *dfu_partition;
    update_finish_cb_t update_finish_callback;
} dfu_core_ctx_t;

#ifndef CI_TEST
ReboootClientUpdateInfo_t client_update_reboot_info __attribute__((section(".noinit")));
#else // CI_TEST
ReboootClientUpdateInfo_t client_update_reboot_info;
#endif // CI_TEST

static dfu_core_ctx_t dfu_ctx;

static QueueHandle_t dfu_event_queue;

static void bm_dfu_send_nop_event(void);
static const libSmState_t* bm_dfu_check_transitions(uint8_t current_state);

static void s_init_run(void);
static void s_idle_entry(void);
static void s_idle_run(void);
static void s_error_run(void) {}
static void s_error_entry(void);

static const libSmState_t dfu_states[BM_NUM_DFU_STATES] = {
    {
        .stateEnum = BM_DFU_STATE_INIT,
        .stateName = "Init", // The name MUST NOT BE NULL
        .run = s_init_run, // This function MUST NOT BE NULL
        .onStateExit = NULL, // This function can be NULL
        .onStateEntry = NULL, // This function can be NULL
    },
    {
        .stateEnum = BM_DFU_STATE_IDLE,
        .stateName = "Idle",
        .run = s_idle_run,
        .onStateExit = NULL,
        .onStateEntry = s_idle_entry,
    },
    {
        .stateEnum = BM_DFU_STATE_ERROR,
        .stateName = "Error",
        .run = s_error_run,
        .onStateExit = NULL,
        .onStateEntry = s_error_entry,
    },
    {
        .stateEnum = BM_DFU_STATE_CLIENT_RECEIVING,
        .stateName = "Client Rx",
        .run = s_client_receiving_run,
        .onStateExit = NULL,
        .onStateEntry = s_client_receiving_entry,
    },
    {
        .stateEnum = BM_DFU_STATE_CLIENT_VALIDATING,
        .stateName = "Client Validating",
        .run = s_client_validating_run,
        .onStateExit = NULL,
        .onStateEntry = s_client_validating_entry,
    },
    {
        .stateEnum = BM_DFU_STATE_CLIENT_REBOOT_REQ,
        .stateName = "Client Reboot Request",
        .run = s_client_reboot_req_run,
        .onStateExit = NULL,
        .onStateEntry = s_client_reboot_req_entry,
    },
    {
        .stateEnum = BM_DFU_STATE_CLIENT_REBOOT_DONE,
        .stateName = "Client Reboot Done",
        .run = s_client_update_done_run,
        .onStateExit = NULL,
        .onStateEntry = s_client_update_done_entry,
    },
    {
        .stateEnum = BM_DFU_STATE_CLIENT_ACTIVATING,
        .stateName = "Client Activating",
        .run = s_client_activating_run,
        .onStateExit = NULL,
        .onStateEntry = s_client_activating_entry,
    },
    {
        .stateEnum = BM_DFU_STATE_HOST_REQ_UPDATE,
        .stateName = "Host Reqeust Update",
        .run = s_host_req_update_run,
        .onStateExit = NULL,
        .onStateEntry = s_host_req_update_entry,
    },
    {
        .stateEnum = BM_DFU_STATE_HOST_UPDATE,
        .stateName = "Host Update",
        .run = s_host_update_run,
        .onStateExit = NULL,
        .onStateEntry = s_host_update_entry,
    },
};

static void bm_dfu_send_nop_event(void) {
    /* Force running the current state by sending a NOP event */
    bm_dfu_event_t evt = {DFU_EVENT_NONE, NULL, 0};
    if(xQueueSend(dfu_event_queue, &evt, 0) != pdTRUE) {
        printf("Message could not be added to Queue\n");
    }
}

static const libSmState_t* bm_dfu_check_transitions(uint8_t current_state){
    if (dfu_ctx.pending_state_change) {
        dfu_ctx.pending_state_change = false;
        return &dfu_states[dfu_ctx.new_state];
    }
    return &dfu_states[current_state];
}

static void s_init_run(void) {
    if (dfu_ctx.current_event.type == DFU_EVENT_INIT_SUCCESS) {
        if (client_update_reboot_info.magic == DFU_REBOOT_MAGIC) {
            bm_dfu_set_pending_state_change(BM_DFU_STATE_CLIENT_REBOOT_DONE);
        } else {
            bm_dfu_set_pending_state_change(BM_DFU_STATE_IDLE);
        }
    }
}

static void s_idle_entry(void) {
    memset(&client_update_reboot_info, 0, sizeof(client_update_reboot_info));
}

static void s_idle_run(void) {
    if (dfu_ctx.current_event.type == DFU_EVENT_RECEIVED_UPDATE_REQUEST) {
        /* Client */
        bm_dfu_client_process_update_request();
    } else if(dfu_ctx.current_event.type == DFU_EVENT_BEGIN_HOST) {
        /* Host */
        dfu_host_start_event_t *start_event = reinterpret_cast<dfu_host_start_event_t*>(dfu_ctx.current_event.buf);
        dfu_ctx.update_finish_callback = start_event->finish_cb;
        bm_dfu_host_set_callback(dfu_ctx.update_finish_callback);
        bm_dfu_host_start_update_timer(start_event->timeoutMs);
        bm_dfu_set_pending_state_change(BM_DFU_STATE_HOST_REQ_UPDATE);
    }
}

/**
 * @brief Entry Function for the Error State
 *
 * @note The Host processes the current error state and either proceeds to the IDLE state or stays in Error (fatal)
 *
 * @param *o    Required by zephyr smf library for state functions
 * @return none
 */
static void s_error_entry(void) {
    switch (dfu_ctx.error) {
        case BM_DFU_ERR_FLASH_ACCESS:
            printf("Flash access error (Fatal Error)\n");
            break;
        case BM_DFU_ERR_IMG_CHUNK_ACCESS:
            printf("Unable to get image chunk\n");
            break;
        case BM_DFU_ERR_TOO_LARGE:
            printf("Image too large for Client\n");
            break;
        case BM_DFU_ERR_SAME_VER:
            printf("Client already loaded with image\n");
            break;
        case BM_DFU_ERR_MISMATCH_LEN:
            printf("Length mismatch\n");
            break;
        case BM_DFU_ERR_BAD_CRC:
            printf("CRC mismatch\n");
            break;
        case BM_DFU_ERR_TIMEOUT:
            printf("DFU Timeout\n");
            break;
        case BM_DFU_ERR_BM_FRAME:
            printf("BM Processing Error\n");
            break;
        case BM_DFU_ERR_ABORTED:
            printf("BM Aborted Error\n");
            break;
        case BM_DFU_ERR_WRONG_VER:
            printf("Client booted with the wrong version.\n");
            break;
        case BM_DFU_ERR_IN_PROGRESS:
            printf("A FW update is already in progress.\n");
            break;
        case BM_DFU_ERR_NONE:
        default:
            break;
    }

    if(dfu_ctx.update_finish_callback) {
        dfu_ctx.update_finish_callback(false, dfu_ctx.error);
    }

    if(dfu_ctx.error <  BM_DFU_ERR_FLASH_ACCESS) {
        bm_dfu_set_pending_state_change(BM_DFU_STATE_IDLE);
    }
    return;
}


/* Consumes any incoming packets from the BCMP service. The payload types are translated
   into events that are placed into the subystem queue and are consumed by the DFU event thread. */
void bm_dfu_process_message(uint8_t *buf, size_t len) {
    configASSERT(buf);
    bm_dfu_event_t evt;
    bm_dfu_frame_t *frame = (bm_dfu_frame_t *)buf;

    /* If this node is not the intended destination, then discard and continue to wait on queue */
    if (dfu_ctx.self_node_id != ((bm_dfu_event_address_t *)frame->payload)->dst_node_id) {
        vPortFree(buf);
        return;
    }

    evt.buf = buf;
    evt.len = len;

    switch (frame->header.frame_type) {
        case BCMP_DFU_START:
            evt.type = DFU_EVENT_RECEIVED_UPDATE_REQUEST;
            printf("Received update request\n");
            if(xQueueSend(dfu_event_queue, &evt, 0) != pdTRUE) {
                vPortFree(buf);
                printf("Message could not be added to Queue\n");
            }
            break;
        case BCMP_DFU_PAYLOAD:
            evt.type = DFU_EVENT_IMAGE_CHUNK;
            printf("Received Payload\n");
            if(xQueueSend(dfu_event_queue, &evt, 0) != pdTRUE) {
                vPortFree(buf);
                printf("Message could not be added to Queue\n");
            }
            break;
        case BCMP_DFU_END:
            evt.type = DFU_EVENT_UPDATE_END;
            printf("Received DFU End\n");
            if(xQueueSend(dfu_event_queue, &evt, 0) != pdTRUE) {
                vPortFree(buf);
                printf("Message could not be added to Queue\n");
            }
            break;
        case BCMP_DFU_ACK:
            evt.type = DFU_EVENT_ACK_RECEIVED;
            printf("Received ACK\n");
            if(xQueueSend(dfu_event_queue, &evt, 0) != pdTRUE) {
                vPortFree(buf);
                printf("Message could not be added to Queue\n");
            }
            break;
        case BCMP_DFU_ABORT:
            evt.type = DFU_EVENT_ABORT;
            printf("Received Abort\n");
            if(xQueueSend(dfu_event_queue, &evt, 0) != pdTRUE) {
                vPortFree(buf);
                printf("Message could not be added to Queue\n");
            }
            break;
        case BCMP_DFU_HEARTBEAT:
            evt.type = DFU_EVENT_HEARTBEAT;
            printf("Received DFU Heartbeat\n");
            if(xQueueSend(dfu_event_queue, &evt, 0) != pdTRUE) {
                vPortFree(buf);
                printf("Message could not be added to Queue\n");
            }
            break;
        case BCMP_DFU_PAYLOAD_REQ:
            evt.type = DFU_EVENT_CHUNK_REQUEST;
            if(xQueueSend(dfu_event_queue, &evt, 0) != pdTRUE) {
                vPortFree(buf);
                printf("Message could not be added to Queue\n");
            }
            break;
        case BCMP_DFU_REBOOT_REQ:
            evt.type = DFU_EVENT_REBOOT_REQUEST;
            if(xQueueSend(dfu_event_queue, &evt, 0) != pdTRUE) {
                vPortFree(buf);
                printf("Message could not be added to Queue\n");
            }
            break;
        case BCMP_DFU_REBOOT:
            evt.type = DFU_EVENT_REBOOT;
            if(xQueueSend(dfu_event_queue, &evt, 0) != pdTRUE) {
                vPortFree(buf);
                printf("Message could not be added to Queue\n");
            }
            break;
        case BCMP_DFU_BOOT_COMPLETE:
            evt.type = DFU_EVENT_BOOT_COMPLETE;
            if(xQueueSend(dfu_event_queue, &evt, 0) != pdTRUE) {
                vPortFree(buf);
                printf("Message could not be added to Queue\n");
            }
            break;
        default:
            configASSERT(false);
        }
}

/**
 * @brief Get DFU Subsystem Event Queue
 *
 * @note Used by DFU host and client contexts to put events into the Subsystem Queue
 *
 * @param none
 * @return QueueHandle_t to DFU Subsystem Zephyr Message Queue
 */
QueueHandle_t bm_dfu_get_event_queue(void) {
    return dfu_event_queue;
}

/**
 * @brief Get latest DFU event
 *
 * @note Get the event currently stored in the DFU Core context
 *
 * @param none
 * @return bm_dfu_event_t Latest DFU Event enum
 */
bm_dfu_event_t bm_dfu_get_current_event(void) {
    return dfu_ctx.current_event;
}

/**
 * @brief Set DFU Core Error
 *
 * @note Set the error of the DFU context which will be used by the Error State logic
 *
 * @param error  Specific DFU Error value
 * @return none
 */
void bm_dfu_set_error(bm_dfu_err_t error) {
    dfu_ctx.error = error;
}

void bm_dfu_set_pending_state_change(uint8_t new_state) {
    dfu_ctx.pending_state_change = 1;
    dfu_ctx.new_state = new_state;
    printf("Transitioning to state: %s\n", dfu_states[new_state].stateName);
    bm_dfu_send_nop_event();
}

/**
 * @brief Send ACK/NACK to Host
 *
 * @note Stuff ACK bm_frame with success and err_code and put into BM Serial TX Queue
 *
 * @param success       1 for ACK, 0 for NACK
 * @param err_code      Error Code enum, read by Host on NACK
 * @return none
 */
void bm_dfu_send_ack(uint64_t dst_node_id, uint8_t success, bm_dfu_err_t err_code) {
    bcmp_dfu_ack_t ack_msg;

    /* Stuff ACK Event */
    ack_msg.ack.success = success;
    ack_msg.ack.err_code = err_code;
    ack_msg.ack.addresses.dst_node_id = dst_node_id;
    ack_msg.ack.addresses.src_node_id = dfu_ctx.self_node_id;
    ack_msg.header.frame_type = BCMP_DFU_ACK;

    if(dfu_ctx.bcmp_dfu_tx(static_cast<bcmp_message_type_t>(ack_msg.header.frame_type), reinterpret_cast<uint8_t*>(&ack_msg), sizeof(ack_msg))){
        printf("Message %d sent \n",ack_msg.header.frame_type);
    } else {
        printf("Failed to send message %d\n",ack_msg.header.frame_type);
    }
}

/**
 * @brief Send Chunk Request
 *
 * @note Stuff Chunk Request bm_frame with chunk number and put into BM Serial TX Queue
 *
 * @param chunk_num     Image Chunk number requested
 * @return none
 */
void bm_dfu_req_next_chunk(uint64_t dst_node_id, uint16_t chunk_num)
{
    bcmp_dfu_payload_req_t chunk_req_msg;

    /* Stuff Chunk Request Event */
    chunk_req_msg.chunk_req.seq_num = chunk_num;
    chunk_req_msg.chunk_req.addresses.src_node_id = dfu_ctx.self_node_id;
    chunk_req_msg.chunk_req.addresses.dst_node_id = dst_node_id;
    chunk_req_msg.header.frame_type = BCMP_DFU_PAYLOAD_REQ;

    if(dfu_ctx.bcmp_dfu_tx(static_cast<bcmp_message_type_t>(chunk_req_msg.header.frame_type), reinterpret_cast<uint8_t*>(&chunk_req_msg), sizeof(chunk_req_msg))){
        printf("Message %d sent \n", chunk_req_msg.header.frame_type);
    } else {
        printf("Failed to send message %d\n", chunk_req_msg.header.frame_type);
    }       
}

/**
 * @brief Send DFU END
 *
 * @note Stuff DFU END bm_frame with success and err_code and put into BM Serial TX Queue
 *
 * @param success       1 for Successful Update, 0 for Unsuccessful
 * @param err_code      Error Code enum, read by Host on Unsuccessful update
 * @return none
 */
void bm_dfu_update_end(uint64_t dst_node_id, uint8_t success, bm_dfu_err_t err_code) {
    bcmp_dfu_end_t update_end_msg;

    /* Stuff Update End Event */
    update_end_msg.result.success = success;
    update_end_msg.result.err_code = err_code;
    update_end_msg.result.addresses.dst_node_id = dst_node_id;
    update_end_msg.result.addresses.src_node_id = dfu_ctx.self_node_id;
    update_end_msg.header.frame_type = BCMP_DFU_END;

    if(dfu_ctx.bcmp_dfu_tx(static_cast<bcmp_message_type_t>(update_end_msg.header.frame_type), reinterpret_cast<uint8_t*>(&update_end_msg), sizeof(update_end_msg))){
        printf("Message %d sent \n",update_end_msg.header.frame_type);
    } else {
        printf("Failed to send message %d\n",update_end_msg.header.frame_type);
    }       
}

/**
 * @brief Send Heartbeat to other device
 *
 * @note Put DFU Heartbeat BM serial frame into BM Serial TX Queue
 *
 * @return none
 */
void bm_dfu_send_heartbeat(uint64_t dst_node_id) {
    bcmp_dfu_heartbeat_t heartbeat_msg;
    heartbeat_msg.addr.dst_node_id = dst_node_id;
    heartbeat_msg.addr.src_node_id = dfu_ctx.self_node_id;
    heartbeat_msg.header.frame_type = BCMP_DFU_HEARTBEAT;

    if(dfu_ctx.bcmp_dfu_tx(static_cast<bcmp_message_type_t>(heartbeat_msg.header.frame_type), reinterpret_cast<uint8_t*>(&heartbeat_msg), sizeof(heartbeat_msg))){
        printf("Message %d sent \n",heartbeat_msg.header.frame_type);
    } else {
        printf("Failed to send message %d\n",heartbeat_msg.header.frame_type);
    }       
}

/* This thread consumes events from the event queue and progresses the state machine */
static void bm_dfu_event_thread(void*) {
    printf("BM DFU Subsystem thread started\n");

    while (1) {
        dfu_ctx.current_event.type = DFU_EVENT_NONE;
        if(xQueueReceive(dfu_event_queue, &dfu_ctx.current_event, portMAX_DELAY) == pdPASS) {
            libSmRun(dfu_ctx.sm_ctx);
        }
        if (dfu_ctx.current_event.buf) {
            vPortFree(dfu_ctx.current_event.buf);
        }
    }
}

void bm_dfu_init(bcmp_dfu_tx_func_t bcmp_dfu_tx, NvmPartition * dfu_partition) {
    configASSERT(bcmp_dfu_tx);
    if(!dfu_partition) {
        printf("Dfu NVM partition not configured, DFU unavailible.\n");
        // TODO - update direct-to-flash
        return;
    }
    dfu_ctx.dfu_partition = dfu_partition;
    dfu_ctx.bcmp_dfu_tx = bcmp_dfu_tx;
    bm_dfu_event_t evt;
    int retval;

    /* Store relevant variables from bristlemouth.c */
    dfu_ctx.self_node_id = getNodeId();

    /* Initialize current event to NULL */
    dfu_ctx.current_event = {DFU_EVENT_NONE, NULL, 0};

    /* Set initial state of DFU State Machine*/
    libSmInit(dfu_ctx.sm_ctx, dfu_states[BM_DFU_STATE_INIT], bm_dfu_check_transitions);

    dfu_event_queue = xQueueCreate( 5, sizeof(bm_dfu_event_t));
    configASSERT(dfu_event_queue);

    bm_dfu_client_init(bcmp_dfu_tx);
    bm_dfu_host_init(bcmp_dfu_tx, dfu_partition);

    evt.type = DFU_EVENT_INIT_SUCCESS;
    evt.buf = NULL;
    evt.len = 0;

    retval = xTaskCreate(bm_dfu_event_thread,
                       "DFU Event",
                       1024,
                       NULL,
                       BM_DFU_EVENT_TASK_PRIORITY,
                       NULL);
    configASSERT(retval == pdPASS);

    if(xQueueSend(dfu_event_queue, &evt, 0) != pdTRUE) {
        printf("Message could not be added to Queue\n");
    }
}

bool bm_dfu_initiate_update(bm_dfu_img_info_t info, uint64_t dest_node_id, update_finish_cb_t update_finish_callback, uint32_t timeoutMs) {
    bool ret = false;
    do {
        if(info.chunk_size > BM_DFU_MAX_CHUNK_SIZE) {
            printf("Invalid chunk size for DFU\n");
            break;
        }
        if(getCurrentStateEnum(dfu_ctx.sm_ctx) != BM_DFU_STATE_IDLE) {
            printf("Not ready to start update.\n");
            if(update_finish_callback) {
                update_finish_callback(false, BM_DFU_ERR_IN_PROGRESS);
            }
            break;
        }
        bm_dfu_event_t evt;
        size_t size = sizeof(dfu_host_start_event_t);
        evt.type = DFU_EVENT_BEGIN_HOST;
        uint8_t *buf = (uint8_t*) pvPortMalloc(size);
        configASSERT(buf);

        dfu_host_start_event_t *start_event = reinterpret_cast<dfu_host_start_event_t*>(buf);
        start_event->start.header.frame_type = BCMP_DFU_START;
        start_event->start.info.addresses.dst_node_id = dest_node_id;
        start_event->start.info.addresses.src_node_id = dfu_ctx.self_node_id;
        memcpy(&start_event->start.info.img_info, &info, sizeof(bm_dfu_img_info_t));
        start_event->finish_cb = update_finish_callback;
        start_event->timeoutMs = timeoutMs;
        evt.buf = buf;
        evt.len = size;
        if(xQueueSend(dfu_event_queue, &evt, 0) != pdTRUE) {
            vPortFree(buf);
            if(update_finish_callback) {
                update_finish_callback(false, BM_DFU_ERR_IN_PROGRESS);
            }
            printf("Message could not be added to Queue\n");
            break;
        }
        ret = true;
    } while(0);
    return ret;
}

bm_dfu_err_t bm_dfu_get_error(void) {
    return dfu_ctx.error;
}


/*!
 * UNIT TEST FUNCTIONS BELOW HERE
 */
#ifdef CI_TEST
libSmContext_t* bm_dfu_test_get_sm_ctx(void) {
    return &dfu_ctx.sm_ctx;
}

void bm_dfu_test_set_dfu_event_and_run_sm(bm_dfu_event_t evt) {
    memcpy(&dfu_ctx.current_event, &evt, sizeof(bm_dfu_event_t));
    libSmRun(dfu_ctx.sm_ctx);
}
#endif //CI_TEST
