#include <string.h>
#include "bristlemouth.h"
#include "bm_dfu.h"
#include "bm_dfu_client.h"
#include "bm_dfu_host.h"
#include "bm_util.h"
#include "bm_ports.h"
#include "lib_state_machine.h"
#include "serial.h"
#include "task_priorities.h"
#include "bsp.h"

#include "semphr.h"

#include "lwip/pbuf.h"

typedef struct dfu_core_ctx_t {
    SerialHandle_t *hSerial;
    libSmContext_t sm_ctx;
    bm_dfu_event_t current_event;
    bool pending_state_change;
    uint8_t new_state;
    bm_dfu_err_t error;
    /* LWIP Specific */
    ip6_addr_t self_addr;
    struct netif* netif;
    struct udp_pcb* pcb;
    uint16_t port;
    struct pbuf* rx_buf;
    volatile uint16_t read_buf_len;
} dfu_core_ctx_t;

static dfu_core_ctx_t dfu_ctx;

static QueueHandle_t dfu_event_queue;
static QueueHandle_t dfu_node_queue;
static SemaphoreHandle_t dfu_rx_sem;

static void bm_dfu_send_nop_event(void);
static const libSmState_t* bm_dfu_check_transitions(uint8_t current_state);
static void bm_dfu_rx_cb(void *arg, struct udp_pcb *upcb, struct pbuf *p, const ip_addr_t *addr, u16_t port);

static void s_init_run(void);
static void s_idle_run(void);
static void s_error_run(void) {}
static void s_error_entry(void);

static uint8_t bm_dfu_serial_magic_num[4] = {0xDE, 0xCA, 0xFB, 0xAD};

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
        .onStateEntry = NULL,
    },
    {
        .stateEnum = BM_DFU_STATE_ERROR,
        .stateName = "Error",
        .run = s_error_run,
        .onStateExit = NULL,
        .onStateEntry = s_error_entry,
    },
    {
        .stateEnum = BM_DFU_STATE_CLIENT,
        .stateName = "Client",
        .run = s_client_run,
        .onStateExit = NULL,
        .onStateEntry = NULL,
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
        .stateEnum = BM_DFU_STATE_CLIENT_ACTIVATING,
        .stateName = "Client Activating",
        .run = s_client_activating_run,
        .onStateExit = NULL,
        .onStateEntry = s_client_activating_entry,
    },
#if BM_DFU_HOST
    {
        .stateEnum = BM_DFU_STATE_HOST,
        .stateName = "Host",
        .run = s_host_run,
        .onStateExit = NULL,
        .onStateEntry = NULL,
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
#endif // BM_DFU_HOST
};

static void bm_dfu_send_nop_event(void) {
    /* Force running the current state by sending a NOP event */
    bm_dfu_event_t evt = {DFU_EVENT_NONE, NULL};
    if(xQueueSend(dfu_event_queue, &evt, 0) != pdTRUE) {
        printf("Message could not be added to Queue\n");
    }
}

#if BM_DFU_HOST
static void bm_dfu_process_rx_byte(void *hSerial, uint8_t byte) {
    static bool await_alignment = true;
    static uint8_t magic_num_idx = 0;
    static bool has_len = false;
    static uint8_t len_idx = 0;
    static uint16_t counter = 0;

    configASSERT(hSerial != NULL);

    /* Waiting for 4-byte magic number */
    if (await_alignment) {
        if(byte == bm_dfu_serial_magic_num[magic_num_idx++]){
            if (magic_num_idx == sizeof(bm_dfu_serial_magic_num)) {
                magic_num_idx = 0;
                await_alignment = false;
                return;
            }
        } else {
            magic_num_idx = 0;
            return;
        }
    /* We found the magic number so we can look for the 2-byte length */
    } else {
        if (!has_len) {
            if (len_idx) {
                dfu_ctx.read_buf_len |= (byte << 8);
                if (dfu_ctx.read_buf_len <= BM_DFU_MAX_FRAME_SIZE) {
                    len_idx = 0;
                    has_len = true;

                    dfu_ctx.rx_buf = pbuf_alloc(PBUF_TRANSPORT, dfu_ctx.read_buf_len, PBUF_RAM);
                    configASSERT(dfu_ctx.rx_buf);
                } else {
                    printf("Incoming Serial RX frame too large for bufffer");
                    dfu_ctx.read_buf_len = 0;
                    len_idx = 0;
                    has_len = false;
                    await_alignment = true;
                }
                return;
            } else {
                // MSB of length field
                dfu_ctx.read_buf_len |= byte;
                len_idx++;
                return;
            }
        /* Have length, so can start storage */
        } else {
            ((uint8_t *) dfu_ctx.rx_buf->payload)[counter++] = byte;
            if (counter == dfu_ctx.read_buf_len) {
                xSemaphoreGive(dfu_rx_sem);

                dfu_ctx.read_buf_len = 0;
                counter = 0;
                has_len = false;
                await_alignment = true;
                return;
            }
        }
    }
}
#endif

static const libSmState_t* bm_dfu_check_transitions(uint8_t current_state){
    if (dfu_ctx.pending_state_change) {
        dfu_ctx.pending_state_change = false;
        return &dfu_states[dfu_ctx.new_state];
    }
    return &dfu_states[current_state];
}

/* Callback function for data received on port 3333 (for DFU node comms)
   We place these messages onto the Node queue to be consumed by the
   node thread */
static void bm_dfu_rx_cb(void *arg, struct udp_pcb *upcb, struct pbuf *buf,
                 const ip_addr_t *addr, u16_t port) {
    (void) arg;
    (void) upcb;
    (void) addr;
    (void) port;
    static bcl_rx_element_t rx_data;

    if (buf != NULL) {
        rx_data.buf = buf;

        /* FIXME: Need to modify address to remove Ingress/Egress port, but is const.
           We are currently not using this */
        // addr->addr[1] &= ~(0xFFFF);
        // rx_data.src = *addr;

        pbuf_ref(buf);
        if(xQueueSend(dfu_node_queue, &rx_data, 0) != pdTRUE) {
            pbuf_free(buf);
            printf("Error sending to Queue\n");
        }
        pbuf_free(buf);
    }
}

static void s_init_run(void) {
    if (dfu_ctx.current_event.type == DFU_EVENT_INIT_SUCCESS) {
        bm_dfu_set_pending_state_change(BM_DFU_STATE_IDLE);
    }
}

static void s_idle_run(void) {
    if (dfu_ctx.current_event.type == DFU_EVENT_RECEIVED_UPDATE_REQUEST) {
        /* Client */
        bm_dfu_client_process_request();
    } else if(dfu_ctx.current_event.type == DFU_EVENT_BEGIN_HOST) {
#if BM_DFU_HOST
        /* Host */
        bm_dfu_set_pending_state_change(BM_DFU_STATE_HOST_REQ_UPDATE);
#endif
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
        case BM_DFU_ERR_NONE:
        default:
            break;
    }

    if(dfu_ctx.error <  BM_DFU_ERR_FLASH_ACCESS) {
        bm_dfu_set_pending_state_change(BM_DFU_STATE_IDLE);
    }
    return;
}


/* Consumes any incoming packets on port 3333 (DFU service). The payload types are translated
   into events that are placed into the subystem queue and are consumed by the DFU event thread. */
static void bm_dfu_node_thread(void*) {
    static bcl_rx_element_t rx_data;
    bm_dfu_event_t evt;

    printf("BM DFU Node thread started");
    while (1) {
        if(xQueueReceive(dfu_node_queue, &rx_data, portMAX_DELAY) == pdPASS) {
            bm_dfu_frame_t *frame = (bm_dfu_frame_t *)rx_data.buf->payload;

            /* If this node is not the intended destination, then discard and continue to wait on queue */
            if (memcmp(dfu_ctx.self_addr.addr, ((bm_dfu_event_address_t *)frame->payload)->dst_addr, sizeof(dfu_ctx.self_addr.addr)) != 0) {
                pbuf_free(rx_data.buf);
                continue;
            }

            evt.pbuf = rx_data.buf;

            switch (frame->header.frame_type) {
                case BM_DFU_START:
                    evt.type = DFU_EVENT_RECEIVED_UPDATE_REQUEST;
                    printf("Received update request\n");
                    pbuf_ref(rx_data.buf);
                    if(xQueueSend(dfu_event_queue, &evt, 0) != pdTRUE) {
                        pbuf_free(rx_data.buf);
                        printf("Message could not be added to Queue\n");
                    }
                    break;
                case BM_DFU_PAYLOAD:
                    evt.type = DFU_EVENT_IMAGE_CHUNK;
                    printf("Received Payload\n");
                    pbuf_ref(rx_data.buf);
                    if(xQueueSend(dfu_event_queue, &evt, 0) != pdTRUE) {
                        pbuf_free(rx_data.buf);
                        printf("Message could not be added to Queue\n");
                    }
                    break;
                case BM_DFU_END:
                    evt.type = DFU_EVENT_UPDATE_END;
                    printf("Received DFU End\n");
                    pbuf_ref(rx_data.buf);
                    if(xQueueSend(dfu_event_queue, &evt, 0) != pdTRUE) {
                        pbuf_free(rx_data.buf);
                        printf("Message could not be added to Queue\n");
                    }
                    break;
                case BM_DFU_ACK:
                    evt.type = DFU_EVENT_ACK_RECEIVED;
                    printf("Received ACK\n");
                    pbuf_ref(rx_data.buf);
                    if(xQueueSend(dfu_event_queue, &evt, 0) != pdTRUE) {
                        pbuf_free(rx_data.buf);
                        printf("Message could not be added to Queue\n");
                    }
                    break;
                case BM_DFU_ABORT:
                    evt.type = DFU_EVENT_ABORT;
                    printf("Received Abort\n");
                    pbuf_ref(rx_data.buf);
                    if(xQueueSend(dfu_event_queue, &evt, 0) != pdTRUE) {
                        pbuf_free(rx_data.buf);
                        printf("Message could not be added to Queue\n");
                    }
                    break;
                case BM_DFU_HEARTBEAT:
                    evt.type = DFU_EVENT_HEARTBEAT;
                    printf("Received DFU Heartbeat\n");
                    pbuf_ref(rx_data.buf);
                    if(xQueueSend(dfu_event_queue, &evt, 0) != pdTRUE) {
                        pbuf_free(rx_data.buf);
                        printf("Message could not be added to Queue\n");
                    }
                    break;
                case BM_DFU_PAYLOAD_REQ:
                    evt.type = DFU_EVENT_CHUNK_REQUEST;
                    pbuf_ref(rx_data.buf);
                    if(xQueueSend(dfu_event_queue, &evt, 0) != pdTRUE) {
                        pbuf_free(rx_data.buf);
                        printf("Message could not be added to Queue\n");
                    }
                    break;
                case BM_DFU_BEGIN_HOST:
                default:
                    break;
            }
            /* Free the allocated pbuf */
            pbuf_free(rx_data.buf);
        }
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
        if (dfu_ctx.current_event.pbuf) {
            pbuf_free(dfu_ctx.current_event.pbuf);
        }
    }
}

#if BM_DFU_HOST
/* This thread consumes events from the event queue and progresses the state machine */
static void bm_dfu_desktop_thread(void*) {
    bm_dfu_event_t evt;
    printf("BM DFU Serial RX thread started\n");

    while (1) {
        dfu_ctx.current_event.type = DFU_EVENT_NONE;
        if(xSemaphoreTake(dfu_rx_sem, portMAX_DELAY) == pdTRUE) {
            bm_dfu_frame_t *frame = (bm_dfu_frame_t *)dfu_ctx.rx_buf->payload;

            evt.pbuf = dfu_ctx.rx_buf;

            switch (frame->header.frame_type) {
                case BM_DFU_BEGIN_HOST:
                    evt.type = DFU_EVENT_BEGIN_HOST;
                    printf("Received DFU Begin from Desktop\n");
                    pbuf_ref(dfu_ctx.rx_buf);
                    if(xQueueSend(dfu_event_queue, &evt, 0) != pdTRUE) {
                        pbuf_free(dfu_ctx.rx_buf);
                        printf("Message could not be added to Queue\n");
                    }
                    pbuf_free(dfu_ctx.rx_buf);
                    break;
                case BM_DFU_PAYLOAD:
                    evt.type = DFU_EVENT_IMAGE_CHUNK;
                    pbuf_ref(dfu_ctx.rx_buf);
                    if(xQueueSend(dfu_event_queue, &evt, 0) != pdTRUE) {
                        pbuf_free(dfu_ctx.rx_buf);
                        printf("Message could not be added to Queue\n");
                    }
                    pbuf_free(dfu_ctx.rx_buf);
                    break;
                case BM_DFU_START:
                case BM_DFU_PAYLOAD_REQ:
                case BM_DFU_END:
                case BM_DFU_ACK:
                case BM_DFU_ABORT:
                case BM_DFU_HEARTBEAT:
                default:
                    printf("Invalid DFU Message received from Desktop");
                    break;
            }
        }
    }
}
#endif

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
    bm_dfu_send_nop_event();
}

/**
 * @brief Send ACK/NACK to Host
 *
 * @note Stuff ACK bm_frame with success and err_code and put into BM Serial TX Queue
 *
 * @param dev_type      Recipient Device (Desktop or End Device)
 * @param success       1 for ACK, 0 for NACK
 * @param err_code      Error Code enum, read by Host on NACK
 * @return none
 */
void bm_dfu_send_ack(uint8_t dev_type, ip6_addr_t* dst_addr, uint8_t success, bm_dfu_err_t err_code) {
    bm_dfu_event_result_t ack_evt;
    uint16_t payload_len;

    /* Stuff ACK Event */
    ack_evt.success = success;
    ack_evt.err_code = err_code;

    if (dst_addr) {
        memcpy(ack_evt.addresses.src_addr, dfu_ctx.self_addr.addr, sizeof(ack_evt.addresses.src_addr));
        memcpy(ack_evt.addresses.dst_addr, dst_addr->addr, sizeof(ack_evt.addresses.dst_addr));
    }

    payload_len = sizeof(bm_dfu_event_result_t) + sizeof(bm_dfu_frame_header_t);

    if (dev_type == BM_NODE) {
        struct pbuf* buf = pbuf_alloc(PBUF_TRANSPORT, payload_len, PBUF_RAM);
        configASSERT(buf);

        bm_dfu_event_t *evtPtr = (bm_dfu_event_t *)buf->payload;
        evtPtr->type = BM_DFU_ACK;

        bm_dfu_frame_header_t *header = (bm_dfu_frame_header_t *)buf->payload;
        memcpy(&header[1], &ack_evt, sizeof(ack_evt));

        udp_sendto_if(dfu_ctx.pcb, buf, &multicast_global_addr, dfu_ctx.port, dfu_ctx.netif);
        pbuf_free(buf);
    } else if (dev_type == BM_DESKTOP) {
        uint8_t* ser_buf = (uint8_t *) pvPortMalloc(sizeof(bm_dfu_serial_header_t) + payload_len);
        configASSERT(ser_buf);
        /* Send Magic Number header */
        memcpy(ser_buf, bm_dfu_serial_magic_num, sizeof(bm_dfu_serial_magic_num));
        /* Send length of payload */
        ((bm_dfu_serial_header_t*) ser_buf)->len = payload_len;
        /* DFU Frame Type */
        ser_buf[sizeof(bm_dfu_serial_header_t)] = BM_DFU_ACK;
        /* DFU Event as Payload */
        memcpy(&ser_buf[sizeof(bm_dfu_serial_header_t)+ sizeof(bm_dfu_frame_header_t)], (uint8_t *) &ack_evt, sizeof(ack_evt));
        serialWriteNocopy(dfu_ctx.hSerial, ser_buf, sizeof(bm_dfu_serial_header_t) + payload_len);
    } else {
        printf("Unsupported Device Type");
    }
}

/**
 * @brief Send Chunk Request
 *
 * @note Stuff Chunk Request bm_frame with chunk number and put into BM Serial TX Queue
 *
 * @param dev_type      Recipient Device (Desktop or End Device)
 * @param chunk_num     Image Chunk number requested
 * @return none
 */
void bm_dfu_req_next_chunk(uint8_t dev_type, ip6_addr_t* dst_addr, uint16_t chunk_num)
{
    bm_dfu_event_chunk_request_t chunk_req_evt;
    uint16_t payload_len;

    /* Stuff Chunk Request Event */
    chunk_req_evt.seq_num = chunk_num;

    if (dst_addr) {
        memcpy(chunk_req_evt.addresses.src_addr, dfu_ctx.self_addr.addr, sizeof(chunk_req_evt.addresses.src_addr));
        memcpy(chunk_req_evt.addresses.dst_addr, dst_addr->addr, sizeof(chunk_req_evt.addresses.dst_addr));
    }

    payload_len = sizeof(bm_dfu_event_chunk_request_t) + sizeof(bm_dfu_frame_header_t);

    if (dev_type == BM_NODE) {
        struct pbuf* buf = pbuf_alloc(PBUF_TRANSPORT, payload_len, PBUF_RAM);
        configASSERT(buf);

        bm_dfu_event_t *evtPtr = (bm_dfu_event_t *)buf->payload;
        evtPtr->type = BM_DFU_PAYLOAD_REQ;

        bm_dfu_frame_header_t *header = (bm_dfu_frame_header_t *)buf->payload;
        memcpy(&header[1], &chunk_req_evt, sizeof(chunk_req_evt));

        udp_sendto_if(dfu_ctx.pcb, buf, &multicast_global_addr, dfu_ctx.port, dfu_ctx.netif);
        pbuf_free(buf);
    } else if (dev_type == BM_DESKTOP) {
        uint8_t* ser_buf = (uint8_t *) pvPortMalloc(sizeof(bm_dfu_serial_header_t) + payload_len);
        configASSERT(ser_buf);
        /* Send Magic Number header */
        memcpy(ser_buf, bm_dfu_serial_magic_num, sizeof(bm_dfu_serial_magic_num));
        /* Send length of payload */
        ((bm_dfu_serial_header_t*) ser_buf)->len = payload_len;
        /* DFU Frame Type */
        ser_buf[sizeof(bm_dfu_serial_header_t)] = BM_DFU_PAYLOAD_REQ;
        /* DFU Event as Payload */
        memcpy(&ser_buf[sizeof(bm_dfu_serial_header_t)+ sizeof(bm_dfu_frame_header_t)], (uint8_t *) &chunk_req_evt, sizeof(chunk_req_evt));
        serialWriteNocopy(dfu_ctx.hSerial, ser_buf, sizeof(bm_dfu_serial_header_t) + payload_len);
    } else {
        printf("Unsupported Device Type");
    }
}

/**
 * @brief Send DFU END
 *
 * @note Stuff DFU END bm_frame with success and err_code and put into BM Serial TX Queue
 *
 * @param dev_type      Recipient Device (Desktop or End Device)
 * @param success       1 for Successful Update, 0 for Unsuccessful
 * @param err_code      Error Code enum, read by Host on Unsuccessful update
 * @return none
 */
void bm_dfu_update_end(uint8_t dev_type, ip6_addr_t* dst_addr, uint8_t success, bm_dfu_err_t err_code) {
    bm_dfu_event_result_t update_end_evt;
    uint16_t payload_len;

    /* Stuff Update End Event */
    update_end_evt.success = success;
    update_end_evt.err_code = err_code;

    if (dst_addr) {
        memcpy(update_end_evt.addresses.src_addr, dfu_ctx.self_addr.addr, sizeof(update_end_evt.addresses.src_addr));
        memcpy(update_end_evt.addresses.dst_addr, dst_addr->addr, sizeof(update_end_evt.addresses.dst_addr));
    }

    payload_len = sizeof(bm_dfu_event_result_t) + sizeof(bm_dfu_frame_header_t);

    if (dev_type == BM_NODE) {
        struct pbuf* buf = pbuf_alloc(PBUF_TRANSPORT, payload_len, PBUF_RAM);
        configASSERT(buf);

        bm_dfu_event_t *evtPtr = (bm_dfu_event_t *)buf->payload;
        evtPtr->type = BM_DFU_END;

        bm_dfu_frame_header_t *header = (bm_dfu_frame_header_t *)buf->payload;
        memcpy(&header[1], &update_end_evt, sizeof(update_end_evt));

        udp_sendto_if(dfu_ctx.pcb, buf, &multicast_global_addr, dfu_ctx.port, dfu_ctx.netif);
        pbuf_free(buf);
    } else if (dev_type == BM_DESKTOP) {
        uint8_t* ser_buf = (uint8_t *) pvPortMalloc(sizeof(bm_dfu_serial_header_t) + payload_len);
        configASSERT(ser_buf);
        /* Send Magic Number header */
        memcpy(ser_buf, bm_dfu_serial_magic_num, sizeof(bm_dfu_serial_magic_num));
        /* Send length of payload */
        ((bm_dfu_serial_header_t*) ser_buf)->len = payload_len;
        /* DFU Frame Type */
        ser_buf[sizeof(bm_dfu_serial_header_t)] = BM_DFU_END;
        /* DFU Event as Payload */
        memcpy(&ser_buf[sizeof(bm_dfu_serial_header_t)+ sizeof(bm_dfu_frame_header_t)], (uint8_t *) &update_end_evt, sizeof(update_end_evt));
        serialWriteNocopy(dfu_ctx.hSerial, ser_buf, sizeof(bm_dfu_serial_header_t) + payload_len);
    } else {
        printf("Unsupported Device Type");
    }

}

/**
 * @brief Send Heartbeat to other device
 *
 * @note Put DFU Heartbeat BM serial frame into BM Serial TX Queue
 *
 * @return none
 */
void bm_dfu_send_heartbeat(ip6_addr_t* dst_addr) {
    bm_dfu_event_address_t heartbeat_evt;

    if (dst_addr) {
        memcpy(heartbeat_evt.src_addr, dfu_ctx.self_addr.addr, sizeof(heartbeat_evt.src_addr));
        memcpy(heartbeat_evt.dst_addr, dst_addr->addr, sizeof(heartbeat_evt.dst_addr));
    } else {
        printf("Dst Addr was NULL\n");
        return;
    }

    struct pbuf* buf = pbuf_alloc(PBUF_TRANSPORT, sizeof(bm_dfu_event_address_t) + sizeof(bm_dfu_frame_header_t), PBUF_RAM);
    configASSERT(buf);

    bm_dfu_event_t *evtPtr = (bm_dfu_event_t *)buf->payload;
    evtPtr->type = BM_DFU_HEARTBEAT;

    bm_dfu_frame_header_t *header = (bm_dfu_frame_header_t *)buf->payload;
    memcpy(&header[1], &heartbeat_evt, sizeof(heartbeat_evt));

    udp_sendto_if(dfu_ctx.pcb, buf, &multicast_global_addr, dfu_ctx.port, dfu_ctx.netif);
    pbuf_free(buf);
}

void bm_dfu_init(SerialHandle_t* hSerial, ip6_addr_t _self_addr, struct netif* _netif) {
    bm_dfu_event_t evt;
    int retval;

    /* Using raw udp tx/rx for now */
    dfu_ctx.pcb = udp_new_ip_type(IPADDR_TYPE_V6);
    dfu_ctx.port = BM_DFU_SOCKET_PORT;
    udp_bind(dfu_ctx.pcb, IP_ANY_TYPE, dfu_ctx.port);
    udp_recv(dfu_ctx.pcb, bm_dfu_rx_cb, NULL);

    /* Store relevant variables from bristlemouth.c */
    dfu_ctx.self_addr = _self_addr;
    dfu_ctx.netif = _netif;

    /* Initialize pbuf to NULL */
    dfu_ctx.current_event = {DFU_EVENT_NONE, NULL};

    /* Set initial state of DFU State Machine*/
    libSmInit(dfu_ctx.sm_ctx, dfu_states[BM_DFU_STATE_INIT], bm_dfu_check_transitions);

    dfu_node_queue = xQueueCreate( 5, sizeof(bcl_rx_element_t));
    dfu_event_queue = xQueueCreate( 5, sizeof(bm_dfu_event_t));

    dfu_rx_sem = xSemaphoreCreateBinary();
    configASSERT(dfu_rx_sem != NULL);

    retval = xTaskCreate(bm_dfu_node_thread,
                       "DFU Node",
                       512,
                       NULL,
                       BM_DFU_NODE_TASK_PRIORITY,
                       NULL);
    configASSERT(retval == pdPASS);

    retval = xTaskCreate(bm_dfu_event_thread,
                       "DFU Event",
                       512,
                       NULL,
                       BM_DFU_EVENT_TASK_PRIORITY,
                       NULL);
    configASSERT(retval == pdPASS);

#if BM_DFU_HOST
    retval = xTaskCreate(bm_dfu_desktop_thread,
                       "DFU Desktop",
                       512,
                       NULL,
                       BM_DFU_DESKTOP_TASK_PRIORITY,
                       NULL);
    configASSERT(retval == pdPASS);

    // hSerial currently required for DFU host
    if(!hSerial) {
        while(1) {
            printf("USB-C must be connected\n");
            for (int i=0; i < 10; i++) {
                IOWrite(&LED_RED, 1);
                vTaskDelay(100);
                IOWrite(&LED_RED, 0);
                IOWrite(&LED_GREEN, 1);
                vTaskDelay(100);
                IOWrite(&LED_GREEN, 0);
            }
        }
    }

    /* Setup processing RX Bytes */
    hSerial->processByte = bm_dfu_process_rx_byte;
    hSerial->txBufferSize = 2048;
    hSerial->txStreamBuffer = xStreamBufferCreate(hSerial->txBufferSize, 1);
    configASSERT(hSerial->txStreamBuffer != NULL);
    hSerial->rxBufferSize = 2048;
    hSerial->rxStreamBuffer = xStreamBufferCreate(hSerial->rxBufferSize, 1);
    configASSERT(hSerial->rxStreamBuffer != NULL);

    dfu_ctx.hSerial = hSerial;

    retval = xTaskCreate(serialGenericRxTask,
                        "DFU Serial",
                        512,
                        hSerial,
                        BM_DFU_SERIAL_RX_TASK_PRIORITY,
                        NULL);
    configASSERT(retval == pdTRUE);

    bm_dfu_host_init(_self_addr, dfu_ctx.pcb, dfu_ctx.port, _netif);
#else
    (void)hSerial;
#endif // BM_DFU_HOST
    bm_dfu_client_init(_self_addr, dfu_ctx.pcb, dfu_ctx.port, _netif);

    evt.type = DFU_EVENT_INIT_SUCCESS;
    evt.pbuf = NULL;

    if(xQueueSend(dfu_event_queue, &evt, 0) != pdTRUE) {
        printf("Message could not be added to Queue\n");
    }
}
