// Includes for FreeRTOS
#include "FreeRTOS.h"
#include "queue.h"
#include "timers.h"
#include "semphr.h"

#include "bm_dfu.h"
#include "bm_dfu_client.h"
#include "bm_util.h"
#include "bootutil/bootutil_public.h"
#include "bootutil/image.h"
#include "crc.h"
#include "flash_map_backend/flash_map_backend.h"
#include "reset_reason.h"
#include "safe_udp.h"
#include "stm32_flash.h"
#include "sysflash/sysflash.h"

typedef struct dfu_client_ctx_t {
    QueueHandle_t dfu_event_queue;
    /* Variables from DFU Start */
    uint32_t image_size;
    uint16_t num_chunks;
    uint16_t crc16;
    uint16_t running_crc16;
    /* Variables from DFU Payload */
    // uint8_t chunk_buf[BM_DFU_MAX_FRAME_SIZE];
    uint16_t chunk_length;
    /* Flash Mem variables */
    const struct flash_area *fa;
    uint16_t img_page_byte_counter;
    uint32_t img_flash_offset;
    uint8_t img_page_buf[BM_IMG_PAGE_LENGTH];
    /* Chunk variables */
    uint8_t chunk_retry_num;
    uint16_t current_chunk;
    TimerHandle_t chunk_timer;
    bool self_update;
    /* network variables */
    ip6_addr_t self_addr;
    struct netif* netif;
    struct udp_pcb* pcb;
    uint16_t port;
    ip6_addr_t host_addr;
} dfu_client_ctx_t;

static dfu_client_ctx_t client_ctx;

static void bm_dfu_client_abort(void);

/**
 * @brief Send DFU Abort to Host
 *
 * @note Stuff DFU Abort bm_frame and put into BM Serial TX Queue
 *
 * @return none
 */
static void bm_dfu_client_abort(void) {
    bm_dfu_event_address_t abort_evt;

    /* Populate the appropriate event */
    memcpy(abort_evt.src_addr, client_ctx.self_addr.addr, sizeof(abort_evt.src_addr));
    memcpy(abort_evt.dst_addr, client_ctx.host_addr.addr, sizeof(abort_evt.dst_addr));

    struct pbuf* buf = pbuf_alloc(PBUF_TRANSPORT, sizeof(bm_dfu_event_result_t) + sizeof(bm_dfu_frame_header_t), PBUF_RAM);
    configASSERT(buf);

    bm_dfu_event_t *evtPtr = (bm_dfu_event_t *)buf->payload;
    evtPtr->type = BM_DFU_ABORT;

    safe_udp_sendto_if(client_ctx.pcb, buf, &multicast_global_addr, client_ctx.port, client_ctx.netif);
    pbuf_free(buf);
}

/**
 * @brief Chunk Timer Handler function
 *
 * @note Puts Chunk Timeout event into DFU Subsystem event queue
 *
 * @param *tmr    Pointer to Zephyr Timer struct
 * @return none
 */
static void chunk_timer_handler(TimerHandle_t tmr) {
    (void) tmr;
    bm_dfu_event_t evt = {DFU_EVENT_CHUNK_TIMEOUT, NULL};

    printf("Chunk Timeout\n");
    if(xQueueSend(client_ctx.dfu_event_queue, &evt, 0) != pdTRUE) {
        printf("Message could not be added to Queue\n");
    }
}

/**
 * @brief Write received chunks to flash
 *
 * @note Stores bytes in a local buffer until a page-worth of bytes can be written to flash.
 *
 * @param man_decode_len    Length of received decoded payload
 * @param man_decode_buf    Buffer of decoded payload
 * @return int32_t 0 on success, non-0 on error
 */
static int32_t bm_dfu_process_payload(uint16_t len, uint8_t * buf)
{
    int32_t retval = 0;

    configASSERT(len);
    configASSERT(buf);

    do {
        if ( BM_IMG_PAGE_LENGTH > (len + client_ctx.img_page_byte_counter)) {
            memcpy(&client_ctx.img_page_buf[client_ctx.img_page_byte_counter], buf, len);
            client_ctx.img_page_byte_counter += len;

            if (client_ctx.img_page_byte_counter == BM_IMG_PAGE_LENGTH) {
                client_ctx.img_page_byte_counter = 0;

                /* Perform page write and increment flash byte counter */
                retval = flash_area_write(client_ctx.fa, client_ctx.img_flash_offset, client_ctx.img_page_buf, BM_IMG_PAGE_LENGTH);
                if (retval) {
                    printf("Unable to write DFU frame to Flash");
                    break;
                } else {
                    client_ctx.img_flash_offset += BM_IMG_PAGE_LENGTH;
                }
            }
        } else {
            uint16_t _remaining_page_length = BM_IMG_PAGE_LENGTH - client_ctx.img_page_byte_counter;
            memcpy(&client_ctx.img_page_buf[client_ctx.img_page_byte_counter], buf, _remaining_page_length);
            client_ctx.img_page_byte_counter += _remaining_page_length;

            if (client_ctx.img_page_byte_counter == BM_IMG_PAGE_LENGTH) {
                client_ctx.img_page_byte_counter = 0;

                /* Perform page write and increment flash byte counter */
                retval = flash_area_write(client_ctx.fa, client_ctx.img_flash_offset, client_ctx.img_page_buf, BM_IMG_PAGE_LENGTH);
                if (retval) {
                    printf("Unable to write DFU frame to Flash");
                    break;
                } else {
                    client_ctx.img_flash_offset += BM_IMG_PAGE_LENGTH;
                }
            }

            /* Memcpy the remaining bytes to next page */
            memcpy(&client_ctx.img_page_buf[client_ctx.img_page_byte_counter], &buf[ _remaining_page_length], (len - _remaining_page_length) );
            client_ctx.img_page_byte_counter += (len - _remaining_page_length);
        }
    } while (0);

    return retval;
}

/**
 * @brief Finish flash writes of final DFU chunk
 *
 * @note Writes dirty bytes in buffer to flash for final chunk
 *
 * @return int32_t  0 on success, non-0 on error
 */
static int32_t bm_dfu_process_end(void) {
    int32_t retval = 0;

    /* If there are any dirty bytes, write to flash */
    if (client_ctx.img_page_byte_counter != 0) {
        /* Perform page write and increment flash byte counter */
        retval = flash_area_write(client_ctx.fa, client_ctx.img_flash_offset, client_ctx.img_page_buf, (client_ctx.img_page_byte_counter));
        if (retval) {
            printf("Unable to write DFU frame to Flash\n");
        } else {
            client_ctx.img_flash_offset += client_ctx.img_page_byte_counter;
        }
    }

    flash_area_close(client_ctx.fa);
    return retval;
}

/**
 * @brief Process a DFU request from the Host
 *
 * @note Client confirms that the update is possible and necessary based on size and version numbers
 *
 * @return none
 */
void bm_dfu_client_process_request(void) {
    uint32_t image_size;
    uint16_t chunk_size;
    //uint8_t minor_version;
    //uint8_t major_version;
    uint32_t image_flash_size;

    bm_dfu_event_t curr_evt = bm_dfu_get_current_event();

    /* Check if we even have a pbuf to inspect */
    if (! curr_evt.pbuf) {
        return;
    }

    bm_dfu_frame_t *frame = (bm_dfu_frame_t *) curr_evt.pbuf->payload;
    bm_dfu_event_img_info_t* img_info_evt = (bm_dfu_event_img_info_t*) &((uint8_t *)frame)[1];

    image_size = img_info_evt->img_info.image_size;
    chunk_size = img_info_evt->img_info.chunk_size;
    //minor_version = curr_evt.event.update_request.img_info.minor_ver;
    //major_version = curr_evt.event.update_request.img_info.major_ver;

    memcpy((uint8_t *) &client_ctx.host_addr, (uint8_t *) img_info_evt->addresses.src_addr, sizeof(img_info_evt->addresses.src_addr));

    /* Check if we are updating ourself */
    if (memcmp(client_ctx.self_addr.addr, client_ctx.host_addr.addr, sizeof(client_ctx.self_addr.addr)) == 0) {
        client_ctx.self_update = true;
    }

    /* TODO: Need to check min/major version numbers */
    if (1) {
        client_ctx.image_size = image_size;

        /* We calculating the number of chunks that the client will be requesting based on the
           size of each chunk and the total size of the image. */
        if (image_size % chunk_size) {
            client_ctx.num_chunks = ( image_size / chunk_size ) + 1;
        } else {
            client_ctx.num_chunks = ( image_size / chunk_size );
        }
        client_ctx.crc16 = img_info_evt->img_info.crc16;

            /* Open the secondary image slot */
        if (flash_area_open(FLASH_AREA_IMAGE_SECONDARY(0), &client_ctx.fa) != 0) {
            if (!client_ctx.self_update) {
                bm_dfu_send_ack(BM_NODE, &client_ctx.host_addr, 0, BM_DFU_ERR_FLASH_ACCESS);
            }
            bm_dfu_set_error(BM_DFU_ERR_FLASH_ACCESS);
            bm_dfu_set_pending_state_change(BM_DFU_STATE_ERROR);

        } else {

            if(client_ctx.fa->fa_size > image_size) {
                /* Erase memory in secondary image slot */
                printf("Erasing flash\n");

                image_flash_size = image_size;
                image_flash_size += (0x2000 - 1);
                image_flash_size &= ~(0x2000 - 1);

                if(flash_area_erase(client_ctx.fa, 0, image_flash_size) != 0) {
                    printf("Error erasing flash!\n");
                    if (!client_ctx.self_update) {
                        bm_dfu_send_ack(BM_NODE, &client_ctx.host_addr, 0, BM_DFU_ERR_FLASH_ACCESS);
                    }
                    bm_dfu_set_error(BM_DFU_ERR_FLASH_ACCESS);
                    bm_dfu_set_pending_state_change(BM_DFU_STATE_ERROR);

                } else {
                    if (!client_ctx.self_update) {
                        bm_dfu_send_ack(BM_NODE, &client_ctx.host_addr, 1, BM_DFU_ERR_NONE);

                        /* (zephyr) TODO: Fix this. Is this needed for FreeRTOS */
                        vTaskDelay(10); // Needed so ACK can properly be sent/processed
                    } else {
                        bm_dfu_send_ack(BM_DESKTOP, NULL, 1, BM_DFU_ERR_NONE);
                    }
                    bm_dfu_set_pending_state_change(BM_DFU_STATE_CLIENT_RECEIVING);

                }
            } else {
                if (!client_ctx.self_update) {
                    bm_dfu_send_ack(BM_NODE, &client_ctx.host_addr, 0, BM_DFU_ERR_TOO_LARGE);
                }
                bm_dfu_set_pending_state_change(BM_DFU_STATE_IDLE);

            }
        }
    } else {
        if (!client_ctx.self_update) {
            bm_dfu_send_ack(BM_NODE, &client_ctx.host_addr, 0, BM_DFU_ERR_SAME_VER);
        }
        bm_dfu_set_pending_state_change(BM_DFU_STATE_IDLE);

    }
}

void s_client_run(void) {}
void s_client_validating_run(void) {}
void s_client_activating_run(void) {}


/**
 * @brief Entry Function for the Client Receiving State
 *
 * @note Client will send the first request for image chunk 0 from the host and kickoff a Chunk timeout timer
 *
 * @param *o    Required by zephyr smf library for state functions
 * @return none
 */
void s_client_receiving_entry(void) {
    /* Start from Chunk #0 */
    client_ctx.current_chunk = 0;
    client_ctx.chunk_retry_num = 0;
    client_ctx.img_page_byte_counter = 0;
    client_ctx.img_flash_offset = 0;
    client_ctx.running_crc16 = 0;

    /* Request Next Chunk */
    if (client_ctx.self_update) {
        bm_dfu_req_next_chunk(BM_DESKTOP, NULL, client_ctx.current_chunk);
    } else {
        bm_dfu_req_next_chunk(BM_NODE, &client_ctx.host_addr, client_ctx.current_chunk);
    }

    /* Kickoff Chunk timeout */
    configASSERT(xTimerStart(client_ctx.chunk_timer, 10));
}

/**
 * @brief Run Function for the Client Receiving State
 *
 * @note Client will periodically request specific image chunks from the Host
 *
 * @param *o    Required by zephyr smf library for state functions
 * @return none
 */
void s_client_receiving_run(void) {
    bm_dfu_event_t curr_evt = bm_dfu_get_current_event();

    if (curr_evt.type == DFU_EVENT_IMAGE_CHUNK) {
        configASSERT(curr_evt.pbuf);
        bm_dfu_frame_t *frame = (bm_dfu_frame_t *) curr_evt.pbuf->payload;
        bm_dfu_event_image_chunk_t* image_chunk_evt = (bm_dfu_event_image_chunk_t*) &((uint8_t *)frame)[1];

        /* Stop Chunk Timer */
        configASSERT(xTimerStop(client_ctx.chunk_timer, 10));

        /* Get Chunk Length and Chunk */
        client_ctx.chunk_length = image_chunk_evt->payload_length;

        /* Calculate Running CRC */
        client_ctx.running_crc16 = crc16_ccitt(client_ctx.running_crc16, image_chunk_evt->payload_buf, client_ctx.chunk_length);

        /* Process the frame */
        if (bm_dfu_process_payload(client_ctx.chunk_length, image_chunk_evt->payload_buf)) {
            bm_dfu_set_error(BM_DFU_ERR_BM_FRAME);
            bm_dfu_set_pending_state_change(BM_DFU_STATE_ERROR);

        }

        /* Request Next Chunk */
        client_ctx.current_chunk++;
        client_ctx.chunk_retry_num = 0;

        if (client_ctx.current_chunk < client_ctx.num_chunks) {
            if (client_ctx.self_update) {
                bm_dfu_req_next_chunk(BM_DESKTOP, NULL, client_ctx.current_chunk);
            } else {
                bm_dfu_req_next_chunk(BM_NODE, &client_ctx.host_addr, client_ctx.current_chunk);
            }
            configASSERT(xTimerStart(client_ctx.chunk_timer, 10));
        } else {
            /* Process the frame */
            if (bm_dfu_process_end()) {
                bm_dfu_set_error(BM_DFU_ERR_BM_FRAME);
                bm_dfu_set_pending_state_change(BM_DFU_STATE_ERROR);

            } else {
                bm_dfu_set_pending_state_change(BM_DFU_STATE_CLIENT_VALIDATING);
            }
        }
    } else if (curr_evt.type == DFU_EVENT_CHUNK_TIMEOUT) {
        client_ctx.chunk_retry_num++;
        /* Try requesting chunk until max retries is reached */
        if (client_ctx.chunk_retry_num >= BM_DFU_MAX_CHUNK_RETRIES) {
            configASSERT(xTimerStop(client_ctx.chunk_timer, 10));
            if (!client_ctx.self_update) {
                bm_dfu_client_abort();
            }
            bm_dfu_set_error(BM_DFU_ERR_TIMEOUT);
            bm_dfu_set_pending_state_change(BM_DFU_STATE_ERROR);

        } else {
            if (client_ctx.self_update) {
                bm_dfu_req_next_chunk(BM_DESKTOP, NULL, client_ctx.current_chunk);
            } else {
                bm_dfu_req_next_chunk(BM_NODE, &client_ctx.host_addr, client_ctx.current_chunk);
            }
            configASSERT(xTimerStart(client_ctx.chunk_timer, 10));
        }
    }
    /* TODO: (IMPLEMENT THIS PERIODICALLY ON HOST SIDE)
       If host is still waiting for chunk, it will send a heartbeat to client */
    else if (curr_evt.type == DFU_EVENT_HEARTBEAT) {
        configASSERT(xTimerStart(client_ctx.chunk_timer, 10));
    }
}

/**
 * @brief Entry Function for the Client Validation State
 *
 * @note If the CRC and image lengths match, move to Client Activation State
 *
 * @param *o    Required by zephyr smf library for state functions
 * @return none
 */
void s_client_validating_entry(void)
{
    /* Verify image length */
    if (client_ctx.image_size != client_ctx.img_flash_offset) {
        printf("Rx Len: %lu, Actual Len: %lu\n", client_ctx.image_size, client_ctx.img_flash_offset);
        if (client_ctx.self_update) {
            bm_dfu_update_end(BM_DESKTOP, NULL, 0, BM_DFU_ERR_MISMATCH_LEN);
        } else {
            bm_dfu_update_end(BM_NODE, &client_ctx.host_addr, 0, BM_DFU_ERR_MISMATCH_LEN);
        }
        bm_dfu_set_error(BM_DFU_ERR_MISMATCH_LEN);
        bm_dfu_set_pending_state_change(BM_DFU_STATE_ERROR);

    } else {
        /* Verify CRC. If ok, then move to Activating state */
        if (client_ctx.crc16 == client_ctx.running_crc16) {

            if (client_ctx.self_update) {
                bm_dfu_update_end(BM_DESKTOP, NULL, 1, BM_DFU_ERR_NONE);
            } else {
                bm_dfu_update_end(BM_NODE, &client_ctx.host_addr, 1, BM_DFU_ERR_NONE);
            }

            bm_dfu_set_pending_state_change(BM_DFU_STATE_CLIENT_ACTIVATING);

        } else {
            printf("Expected Image CRC: %d | Calculated Image CRC: %d\n", client_ctx.crc16, client_ctx.running_crc16);
            if (client_ctx.self_update) {
                bm_dfu_update_end(BM_DESKTOP, NULL, 0, BM_DFU_ERR_BAD_CRC);
            } else {
                bm_dfu_update_end(BM_NODE, &client_ctx.host_addr, 0, BM_DFU_ERR_BAD_CRC);
            }
            bm_dfu_set_error(BM_DFU_ERR_BAD_CRC);
            bm_dfu_set_pending_state_change(BM_DFU_STATE_ERROR);
        }
    }
}

/**
 * @brief Entry Function for the Client Activating State
 *
 * @note Upon Validation of received image, the device will set pending image bit and reboot
 *
 * @param *o    Required by zephyr smf library for state functions
 * @return none
 */
void s_client_activating_entry(void)
{
    /* Set as temporary switch. New application must confirm or else MCUBoot will
    switch back to old image */
    printf("Successful transfer. Should be resetting\n");

    /* Add a small delay so DFU_END message can get out to (Host Node + Desktop) before resetting device */
    vTaskDelay(10);

    boot_set_pending(0);
    resetSystem(RESET_REASON_MCUBOOT);
}

/**
 * @brief Initialization function for the DFU Client subsystem
 *
 * @note Gets relevant message queues and semaphores, and creates Chunk Timeout Timer
 *
 * @param sock    Socket from DFU Core
 * @return none
 */
void bm_dfu_client_init(struct udp_pcb* _pcb, uint16_t _port, struct netif* _netif)
{
    int32_t tmr_id = 0;

    /* Store relevant variables */
    client_ctx.self_addr = *netif_ip6_addr(_netif, 0);
    client_ctx.pcb = _pcb;
    client_ctx.port = _port;
    client_ctx.netif = _netif;

    /* Get DFU Subsystem Queue */
    client_ctx.dfu_event_queue = bm_dfu_get_event_queue();

    /* Assume not a self update */
    client_ctx.self_update = false;

    client_ctx.chunk_timer = xTimerCreate("DFU Client Chunk Timer", (BM_DFU_CLIENT_CHUNK_TIMEOUT_MS / portTICK_RATE_MS),
                                      pdTRUE, (void *) &tmr_id, chunk_timer_handler);
    configASSERT(client_ctx.chunk_timer);
}
