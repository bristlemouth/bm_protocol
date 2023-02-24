#pragma once

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

// Includes for FreeRTOS
#include "FreeRTOS.h"
#include "queue.h"

// lwip
#include "lwip/init.h"
#include "lwip/tcpip.h"
#include "lwip/udp.h"
#include "lwip/inet.h"

#include "serial.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define BM_DFU_SOCKET_PORT          3333
#define BM_DFU_MAX_FRAME_SIZE       600 // Max DFU Chunk size is 512
#define BM_DFU_MAX_CHUNK_RETRIES    5

#define BM_IMG_PAGE_LENGTH          2048

enum BM_DEV {
    BM_DESKTOP,
    BM_NODE,
};

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
    // All errors below this are "fatal"
    BM_DFU_ERR_FLASH_ACCESS,
} bm_dfu_err_t;

enum BM_DFU_FRM_TYPE {
    BM_DFU_START,
    BM_DFU_PAYLOAD_REQ,
    BM_DFU_PAYLOAD,
    BM_DFU_END,
    BM_DFU_ACK,
    BM_DFU_ABORT,
    BM_DFU_HEARTBEAT,
    BM_DFU_BEGIN_HOST,
};

enum BM_DFU_HFSM_STATES { 
    BM_DFU_STATE_INIT, 
    BM_DFU_STATE_IDLE,
    BM_DFU_STATE_ERROR,
    BM_DFU_STATE_CLIENT,
    BM_DFU_STATE_CLIENT_RECEIVING,
    BM_DFU_STATE_CLIENT_VALIDATING,
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
};

typedef struct __attribute__((__packed__)) bm_dfu_img_info_s {
    uint32_t image_size;
    uint16_t chunk_size;
    uint16_t crc16;
    uint8_t major_ver;
    uint8_t minor_ver;
} bm_dfu_img_info_t;

typedef struct __attribute__((__packed__)) bm_dfu_frame_header_s {
    uint8_t frame_type;
} bm_dfu_frame_header_t;

typedef struct __attribute__((__packed__)) bm_dfu_frame_s {
    bm_dfu_frame_header_t header;
    uint8_t payload[0];
} bm_dfu_frame_t;

typedef struct __attribute__((__packed__)) bm_dfu_serial_header_s {
    uint32_t magic_num;
    uint16_t len;
} bm_dfu_serial_header_t;

typedef struct __attribute__((__packed__)) bm_dfu_event_init_success_s {
    uint8_t reserved;
}bm_dfu_event_init_success_t;

typedef struct __attribute__((__packed__)) bm_dfu_address_s {
    uint32_t src_addr[4];
    uint32_t dst_addr[4];
} bm_dfu_event_address_t;

typedef struct __attribute__((__packed__)) bm_dfu_event_chunk_request_s {
    bm_dfu_event_address_t addresses;
    uint16_t seq_num;
} bm_dfu_event_chunk_request_t;

typedef struct __attribute__((__packed__)) bm_dfu_event_image_chunk_s {
    bm_dfu_event_address_t addresses;
    uint16_t payload_length;
    uint8_t payload_buf[0];
} bm_dfu_event_image_chunk_t;

typedef struct __attribute__((__packed__)) bm_dfu_result_s {
    bm_dfu_event_address_t addresses;
    uint8_t success;
    uint8_t err_code;
} bm_dfu_event_result_t;

typedef struct __attribute__((__packed__)) bm_dfu_event_img_info_s {
    bm_dfu_event_address_t addresses;
    bm_dfu_img_info_t img_info;
} bm_dfu_event_img_info_t;

typedef struct {
    uint8_t type;
    struct pbuf *pbuf;
} bm_dfu_event_t; 

QueueHandle_t bm_dfu_get_event_queue(void);
bm_dfu_event_t bm_dfu_get_current_event(void);
void bm_dfu_set_error(bm_dfu_err_t error);
void bm_dfu_set_pending_state_change(uint8_t new_state);

void bm_dfu_send_ack(uint8_t dev_type, ip6_addr_t* dst_addr, uint8_t success, bm_dfu_err_t err_code);
void bm_dfu_req_next_chunk(uint8_t dev_type, ip6_addr_t* dst_addr, uint16_t chunk_num);
void bm_dfu_update_end(uint8_t dev_type, ip6_addr_t* dst_addr, uint8_t success, bm_dfu_err_t err_code);
void bm_dfu_send_heartbeat(ip6_addr_t* dst_addr);

void bm_dfu_init(SerialHandle_t* hSerial, ip6_addr_t _self_addr, struct netif* _netif);

#ifdef __cplusplus
}
#endif