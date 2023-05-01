#pragma once

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

// Includes for FreeRTOS
#include "FreeRTOS.h"
#include "queue.h"

#include "bcmp_messages.h"
#include "nvmPartition.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define BM_DFU_MAX_CHUNK_SIZE       (1024)
#define BM_DFU_MAX_CHUNK_RETRIES    5

#define BM_IMG_PAGE_LENGTH          2048

typedef enum {
    BM_DFU_ERR_NONE,
    BM_DFU_ERR_TOO_LARGE,
    BM_DFU_ERR_SAME_VER,
    BM_DFU_ERR_MISMATCH_LEN,
    BM_DFU_ERR_BAD_CRC,
    BM_DFU_ERR_IMG_CHUNK_ACCESS,
    BM_DFU_ERR_TIMEOUT,
    BM_DFU_ERR_BM_FRAME,
    BM_DFU_ERR_ABORTED,
    BM_DFU_ERR_WRONG_VER,
    // All errors below this are "fatal"
    BM_DFU_ERR_FLASH_ACCESS,
} bm_dfu_err_t;

enum BM_DFU_HFSM_STATES {
    BM_DFU_STATE_INIT,
    BM_DFU_STATE_IDLE,
    BM_DFU_STATE_ERROR,
    BM_DFU_STATE_CLIENT,
    BM_DFU_STATE_CLIENT_RECEIVING,
    BM_DFU_STATE_CLIENT_VALIDATING,
    BM_DFU_STATE_CLIENT_REBOOT_REQ,
    BM_DFU_STATE_CLIENT_REBOOT_DONE,
    BM_DFU_STATE_CLIENT_ACTIVATING,
    BM_DFU_STATE_HOST,
    BM_DFU_STATE_HOST_REQ_UPDATE,
    BM_DFU_STATE_HOST_UPDATE,
    BM_NUM_DFU_STATES,
};

enum BM_DFU_EVT_TYPE {
    DFU_EVENT_NONE,
    DFU_EVENT_INIT_SUCCESS,
    DFU_EVENT_BEGIN_UPDATE,
    DFU_EVENT_RECEIVED_UPDATE_REQUEST,
    DFU_EVENT_CHUNK_REQUEST,
    DFU_EVENT_IMAGE_CHUNK,
    DFU_EVENT_UPDATE_END,
    DFU_EVENT_ACK_RECEIVED,
    DFU_EVENT_ACK_TIMEOUT,
    DFU_EVENT_CHUNK_TIMEOUT,
    DFU_EVENT_HEARTBEAT_TIMEOUT,
    DFU_EVENT_HEARTBEAT,
    DFU_EVENT_ABORT,
    DFU_EVENT_BEGIN_HOST,
    DFU_EVENT_REBOOT_REQUEST,
    DFU_EVENT_REBOOT,
    DFU_EVENT_BOOT_COMPLETE,
};

typedef struct {
    uint8_t type;
    uint8_t * buf;
    size_t len;
} bm_dfu_event_t;

#define DFU_REBOOT_MAGIC (0xBADC0FFE)

typedef struct  __attribute__((__packed__)) {
  uint32_t magic;
  uint8_t major;
  uint8_t minor;
  uint64_t host_node_id;
} ReboootClientUpdateInfo_t;

extern ReboootClientUpdateInfo_t client_update_reboot_info __attribute__((section(".noinit")));

typedef bool (*bcmp_dfu_tx_func_t)(bcmp_message_type_t type, uint8_t *buff, uint16_t len);
typedef void (*update_finish_cb_t)(bool success);

QueueHandle_t bm_dfu_get_event_queue(void);
bm_dfu_event_t bm_dfu_get_current_event(void);
void bm_dfu_set_error(bm_dfu_err_t error);
void bm_dfu_set_pending_state_change(uint8_t new_state);

void bm_dfu_send_ack(uint64_t dst_node_id, uint8_t success, bm_dfu_err_t err_code);
void bm_dfu_req_next_chunk(uint64_t dst_node_id, uint16_t chunk_num);
void bm_dfu_update_end(uint64_t dst_node_id, uint8_t success, bm_dfu_err_t err_code);
void bm_dfu_send_heartbeat(uint64_t dst_node_id);

void bm_dfu_init(bcmp_dfu_tx_func_t bcmp_dfu_tx, NvmPartition * dfu_partition);
void bm_dfu_process_message(uint8_t * buf, size_t len);
bool bm_dfu_initiate_update(bm_dfu_img_info_t info, uint64_t dest_node_id, update_finish_cb_t update_finish_callback, uint32_t timeoutMs );

#ifdef __cplusplus
}
#endif
