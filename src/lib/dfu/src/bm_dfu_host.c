#include <stdio.h>
#include <string.h>
#include "bm_dfu.h"
#include "bm_dfu_host.h"
#include "bm_util.h"
#include "safe_udp.h"

typedef struct dfu_host_ctx_t {
    QueueHandle_t dfu_event_queue;
    TimerHandle_t ack_timer;
    uint8_t ack_retry_num;
    TimerHandle_t heartbeat_timer;
    bm_dfu_img_info_t img_info;
    uint8_t* chunk_buf;
    uint16_t chunk_length;
    ip6_addr_t self_addr;
    struct netif* netif;
    struct udp_pcb* pcb;
    uint16_t port;
    ip6_addr_t client_addr;
} dfu_host_ctx_t;

static dfu_host_ctx_t host_ctx;

static void ack_timer_handler(TimerHandle_t tmr);
static void heartbeat_timer_handler(TimerHandle_t tmr);

static void bm_dfu_host_req_update(bool self_update);

void s_host_run(void) {}

/**
 * @brief ACK Timer Handler function
 *
 * @note Puts ACK Timeout event into DFU Subsystem event queue
 *
 * @param *tmr    Pointer to Zephyr Timer struct
 * @return none
 */
static void ack_timer_handler(TimerHandle_t tmr) {
    (void) tmr;
    bm_dfu_event_t evt = {DFU_EVENT_ACK_TIMEOUT, NULL};

    printf("Ack Timeout\n");
    if(xQueueSend(host_ctx.dfu_event_queue, &evt, 0) != pdTRUE) {
        printf("Message could not be added to Queue\n");
    }
}

/**
 * @brief Heartbeat Timer Handler function
 *
 * @note Puts Heartbeat Timeout event into DFU Subsystem event queue
 *
 * @param *tmr    Pointer to Zephyr Timer struct
 * @return none
 */
static void heartbeat_timer_handler(TimerHandle_t tmr) {
    (void) tmr;
    bm_dfu_event_t evt = {DFU_EVENT_HEARTBEAT_TIMEOUT, NULL};

    printf("Heartbeat Timeout\n");
    if(xQueueSend(host_ctx.dfu_event_queue, &evt, 0) != pdTRUE) {
        printf("Message could not be added to Queue\n");
    }
}

/**
 * @brief Send Request Update to Client
 *
 * @note Stuff Update Request bm_frame with image info and put into BM Serial TX Queue
 *
 * @return none
 */
static void bm_dfu_host_req_update(bool self_update) {
    bm_dfu_event_img_info_t update_req_evt;

    printf("Sending Update to Client\n");

    /* Populate the appropriate event */
    update_req_evt.img_info = host_ctx.img_info;
    memcpy(update_req_evt.addresses.src_addr, host_ctx.self_addr.addr, sizeof(update_req_evt.addresses.src_addr));
    memcpy(update_req_evt.addresses.dst_addr, host_ctx.client_addr.addr, sizeof(update_req_evt.addresses.dst_addr));

    if (self_update) {
        bm_dfu_event_t evt = {DFU_EVENT_RECEIVED_UPDATE_REQUEST, NULL};
        evt.pbuf = pbuf_alloc(PBUF_TRANSPORT, sizeof(bm_dfu_event_img_info_t) + sizeof(bm_dfu_frame_header_t), PBUF_RAM);
        configASSERT(evt.pbuf);

        evt.type = DFU_EVENT_RECEIVED_UPDATE_REQUEST;

        bm_dfu_frame_header_t *header = (bm_dfu_frame_header_t *)evt.pbuf->payload;
        header->frame_type = BM_DFU_START;
        memcpy(&header[1], &update_req_evt, sizeof(update_req_evt));
        bm_dfu_set_pending_state_change(BM_DFU_STATE_IDLE);

        pbuf_ref(evt.pbuf);
        if(xQueueSend(host_ctx.dfu_event_queue, &evt, 0) != pdTRUE) {
            printf("Message could not be added to Queue\n");
            pbuf_free(evt.pbuf);
        }
        pbuf_free(evt.pbuf);
    } else {
        struct pbuf* buf = pbuf_alloc(PBUF_TRANSPORT, sizeof(bm_dfu_event_img_info_t) + sizeof(bm_dfu_frame_header_t), PBUF_RAM);
        configASSERT(buf);

        bm_dfu_event_t *evtPtr = (bm_dfu_event_t *)buf->payload;
        evtPtr->type = BM_DFU_START;

        bm_dfu_frame_header_t *header = (bm_dfu_frame_header_t *)buf->payload;
        memcpy(&header[1], &update_req_evt, sizeof(update_req_evt));

        safe_udp_sendto_if(host_ctx.pcb, buf, &multicast_global_addr, host_ctx.port, host_ctx.netif);
        pbuf_free(buf);
    }
}

/**
 * @brief Send Chunk to Client
 *
 * @note Stuff bm_frame with image chunk and put into BM Serial TX Queue
 *
 * @return none
 */
static void bm_dfu_host_send_chunk(void) {
    bm_dfu_event_t evt = bm_dfu_get_current_event();
    bm_dfu_frame_t *frame = (bm_dfu_frame_t *) evt.pbuf->payload;
    bm_dfu_event_image_chunk_t* chunk_evt =  (bm_dfu_event_image_chunk_t*) &((uint8_t *)frame)[1];

    memcpy(chunk_evt->addresses.src_addr, host_ctx.self_addr.addr, sizeof(host_ctx.self_addr.addr));
    memcpy(chunk_evt->addresses.dst_addr, host_ctx.client_addr.addr, sizeof(host_ctx.client_addr.addr));

    safe_udp_sendto_if(host_ctx.pcb, evt.pbuf, &multicast_global_addr, host_ctx.port, host_ctx.netif);
}

/**
 * @brief Entry Function for the Request Update State
 *
 * @note The Host sends an update request to the client and starts the ACK timeout timer
 *
 * @param *o    Required by zephyr smf library for state functions
 * @return none
 */
void s_host_req_update_entry(void) {
    bm_dfu_event_t curr_evt = bm_dfu_get_current_event();

    /* Check if we even have a pbuf to inspect */
    if (! curr_evt.pbuf) {
        return;
    }

    bm_dfu_frame_t *frame = (bm_dfu_frame_t *) curr_evt.pbuf->payload;
    bm_dfu_event_img_info_t* img_info_evt = (bm_dfu_event_img_info_t*) &((uint8_t *)frame)[1];
    host_ctx.img_info = img_info_evt->img_info;

    memcpy(host_ctx.client_addr.addr, img_info_evt->addresses.dst_addr, sizeof(img_info_evt->addresses.dst_addr));

    printf("DFU Client IPv6 Address: %08lx:%08lx:%08lx:%08lx\n", host_ctx.client_addr.addr[0],
                                                             host_ctx.client_addr.addr[1],
                                                             host_ctx.client_addr.addr[2],
                                                             host_ctx.client_addr.addr[3]);

    /* Check if Host is being updated */
    if (memcmp(host_ctx.self_addr.addr, host_ctx.client_addr.addr, sizeof(host_ctx.self_addr.addr)) == 0) {
        printf("Self update\n");
        bm_dfu_host_req_update(true);
    } else {
        host_ctx.ack_retry_num = 0;
        /* Request Client Firmware Update */
        bm_dfu_host_req_update(false);

        /* Kickoff ACK timeout */
        configASSERT(xTimerStart(host_ctx.ack_timer, 10));
    }
}

/**
 * @brief Run Function for the Request Update State
 *
 * @note The state is waiting on an ACK from the client to begin the update. Returns to idle state on timeout
 *
 * @param *o    Required by zephyr smf library for state functions
 * @return none
 */
void s_host_req_update_run(void)
{
    bm_dfu_event_t curr_evt = bm_dfu_get_current_event();

    if (curr_evt.type == DFU_EVENT_ACK_RECEIVED) {
        configASSERT(curr_evt.pbuf);
        bm_dfu_frame_t *frame = (bm_dfu_frame_t *) curr_evt.pbuf->payload;
        bm_dfu_event_result_t* result_evt = (bm_dfu_event_result_t*) &((uint8_t *)frame)[1];

        /* Stop ACK Timer */
        configASSERT(xTimerStop(host_ctx.ack_timer, 10));

        bm_dfu_send_ack(BM_DESKTOP, NULL, result_evt->success, result_evt->err_code);

        if (result_evt->success) {
            bm_dfu_set_pending_state_change(BM_DFU_STATE_HOST_UPDATE);
        } else {
            bm_dfu_set_error(result_evt->err_code);
            bm_dfu_set_pending_state_change(BM_DFU_STATE_ERROR);
        }
    } else if (curr_evt.type == DFU_EVENT_ACK_TIMEOUT) {
        host_ctx.ack_retry_num++;

        /* Wait for ack until max retries is reached */
        if (host_ctx.ack_retry_num >= BM_DFU_MAX_ACK_RETRIES) {
            configASSERT(xTimerStop(host_ctx.ack_timer, 10));
            bm_dfu_send_ack(BM_DESKTOP, NULL, 0, BM_DFU_ERR_TIMEOUT);
            bm_dfu_set_error(BM_DFU_ERR_TIMEOUT);
            bm_dfu_set_pending_state_change(BM_DFU_STATE_ERROR);
        } else {
            bm_dfu_host_req_update(false);
            configASSERT(xTimerStart(host_ctx.ack_timer, 10));
        }
    }
}

/**
 * @brief Entry Function for the Update State
 *
 * @note The Host starts the Heartbeat timeout timer
 *
 * @return none
 */
void s_host_update_entry(void) {
    configASSERT(xTimerStart(host_ctx.heartbeat_timer, 10));
}

/**
 * @brief Run Function for the Update State
 *
 * @note Host state that sends chunks of image to Client. Exits on heartbeat timeout or end message received from client
 *
 * @return none
 */
void s_host_update_run(void) {
    bm_dfu_frame_t *frame = NULL;
    bm_dfu_event_t curr_evt = bm_dfu_get_current_event();

    /* Check if we even have a pbuf to inspect */
    if (curr_evt.pbuf) {
        frame = (bm_dfu_frame_t *) curr_evt.pbuf->payload;
    }

    if (curr_evt.type == DFU_EVENT_CHUNK_REQUEST) {
        configASSERT(frame);
        bm_dfu_event_chunk_request_t* chunk_req_evt = (bm_dfu_event_chunk_request_t*) &((uint8_t *)frame)[1];

        configASSERT(xTimerStop(host_ctx.heartbeat_timer, 10));

        /* Request Next Chunk */
        bm_dfu_req_next_chunk(BM_DESKTOP, NULL, chunk_req_evt->seq_num);

        /* Send Heartbeat to Client
            TODO: Make this a periodic heartbeat in case it takes a while to grab chunk from external host
        */
        bm_dfu_send_heartbeat(&host_ctx.client_addr);
    } else if (curr_evt.type == DFU_EVENT_IMAGE_CHUNK) {
        /* resend the frame to the client as is */
        bm_dfu_host_send_chunk();

        configASSERT(xTimerStart(host_ctx.heartbeat_timer, 10));
    } else if (curr_evt.type == DFU_EVENT_UPDATE_END) {
        configASSERT(frame);
        bm_dfu_event_result_t* update_end_evt = (bm_dfu_event_result_t*) &((uint8_t *)frame)[1];

        configASSERT(xTimerStop(host_ctx.heartbeat_timer, 10));
        bm_dfu_update_end(BM_DESKTOP, NULL, update_end_evt->success, update_end_evt->err_code);

        if (update_end_evt->success) {
            printf("Successfully updated Client\n");
        } else {
            printf("Client Update Failed\n");
        }
        bm_dfu_set_pending_state_change(BM_DFU_STATE_IDLE);
    } else if (curr_evt.type == DFU_EVENT_HEARTBEAT) {
        configASSERT(xTimerStart(host_ctx.heartbeat_timer, 10));
    } else if (curr_evt.type == DFU_EVENT_HEARTBEAT_TIMEOUT) {
        bm_dfu_update_end(BM_DESKTOP, NULL, 0, BM_DFU_ERR_TIMEOUT);
        bm_dfu_set_error(BM_DFU_ERR_TIMEOUT);
        bm_dfu_set_pending_state_change(BM_DFU_STATE_ERROR);
    } else if (curr_evt.type == DFU_EVENT_ABORT) {
        bm_dfu_update_end(BM_DESKTOP, NULL, 0, BM_DFU_ERR_ABORTED);
        bm_dfu_set_pending_state_change(BM_DFU_STATE_IDLE);
    }
}

void bm_dfu_host_init(ip6_addr_t _self_addr, struct udp_pcb* _pcb, uint16_t _port, struct netif* _netif) {
    int tmr_id = 0;

    /* Store relevant variables */
    host_ctx.self_addr = _self_addr;
    host_ctx.pcb = _pcb;
    host_ctx.port = _port;
    host_ctx.netif = _netif;

    /* Get DFU Subsystem Queue */
    host_ctx.dfu_event_queue = bm_dfu_get_event_queue();

    /* Initialize ACK and Heartbeat Timer */
    host_ctx.ack_timer = xTimerCreate("DFU Host Ack", (BM_DFU_HOST_ACK_TIMEOUT_MS / portTICK_RATE_MS),
                                      pdTRUE, (void *) &tmr_id, ack_timer_handler);
    configASSERT(host_ctx.ack_timer);

    host_ctx.heartbeat_timer = xTimerCreate("DFU Host Heartbeat", (BM_DFU_HOST_HEARTBEAT_TIMEOUT_MS / portTICK_RATE_MS),
                                      pdTRUE, (void *) &tmr_id, heartbeat_timer_handler);
    configASSERT(host_ctx.heartbeat_timer);
}
