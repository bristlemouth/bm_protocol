// Includes for FreeRTOS
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include "FreeRTOS.h"
#include "queue.h"
#include "timers.h"
#include "semphr.h"

#include "bm_dfu.h"
#include "bm_dfu_client.h"
#include "bootutil/bootutil_public.h"
#include "bootutil/image.h"
#include "flash_map_backend/flash_map_backend.h"
#include "stm32_flash.h"
#include "sysflash/sysflash.h"
#include "crc.h"
#include "reset_reason.h"
#include "device_info.h"

typedef struct dfu_client_ctx_t {
    QueueHandle_t dfu_event_queue;
    /* Variables from DFU Start */
    uint32_t image_size;
    uint16_t num_chunks;
    uint16_t crc16;
    uint16_t running_crc16;
    /* Variables from DFU Payload */
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
    uint64_t self_node_id;
    uint64_t host_node_id;
    bcmp_dfu_tx_func_t bcmp_dfu_tx;
} dfu_client_ctx_t;

static dfu_client_ctx_t client_ctx;

static void bm_dfu_client_abort(void);
static void bm_dfu_client_send_reboot_request();
static void bm_dfu_client_send_boot_complete(uint64_t host_node_id);
static void bm_dfu_client_transition_to_error(bm_dfu_err_t err);

/**
 * @brief Send DFU Abort to Host
 *
 * @note Stuff DFU Abort bm_frame and put into BM Serial TX Queue
 *
 * @return none
 */
static void bm_dfu_client_abort(void) {
    bcmp_dfu_abort_t abort_msg;

    /* Populate the appropriate event */
    abort_msg.err.addresses.dst_node_id = client_ctx.host_node_id;
    abort_msg.err.addresses.src_node_id = client_ctx.self_node_id;
    abort_msg.err.err_code = BM_DFU_ERR_ABORTED;
    abort_msg.err.success = 0;
    abort_msg.header.frame_type = BCMP_DFU_ABORT;
    if(client_ctx.bcmp_dfu_tx(static_cast<bcmp_message_type_t>(abort_msg.header.frame_type), reinterpret_cast<uint8_t*>(&abort_msg), sizeof(abort_msg))){
        printf("Message %d sent \n",abort_msg.header.frame_type);
    } else {
        printf("Failed to send message %d\n",abort_msg.header.frame_type);
    }
}

static void bm_dfu_client_send_reboot_request() {
    bcmp_dfu_reboot_req_t reboot_req;
    reboot_req.addr.src_node_id = client_ctx.self_node_id;
    reboot_req.addr.dst_node_id = client_ctx.host_node_id;
    reboot_req.header.frame_type = BCMP_DFU_REBOOT_REQ;
    if(client_ctx.bcmp_dfu_tx(static_cast<bcmp_message_type_t>(reboot_req.header.frame_type), reinterpret_cast<uint8_t*>(&reboot_req), sizeof(bcmp_dfu_reboot_req_t))){
        printf("Message %d sent \n",reboot_req.header.frame_type);
    } else {
        printf("Failed to send message %d\n",reboot_req.header.frame_type);
    }
}

static void bm_dfu_client_send_boot_complete(uint64_t host_node_id) {
    bcmp_dfu_boot_complete_t boot_compl;
    boot_compl.addr.src_node_id = client_ctx.self_node_id;
    boot_compl.addr.dst_node_id = host_node_id;
    boot_compl.header.frame_type = BCMP_DFU_BOOT_COMPLETE;
    if(client_ctx.bcmp_dfu_tx(static_cast<bcmp_message_type_t>(boot_compl.header.frame_type), reinterpret_cast<uint8_t*>(&boot_compl), sizeof(bcmp_dfu_boot_complete_t))){
        printf("Message %d sent \n",boot_compl.header.frame_type);
    } else {
        printf("Failed to send message %d\n",boot_compl.header.frame_type);
    }
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
    bm_dfu_event_t evt = {DFU_EVENT_CHUNK_TIMEOUT, NULL, 0};

    if(xQueueSend(client_ctx.dfu_event_queue, &evt, 0) != pdTRUE) {
        configASSERT(false);
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
void bm_dfu_client_process_update_request(void) {
    uint32_t image_size;
    uint16_t chunk_size;
    uint8_t minor_version;
    uint8_t major_version;
    uint32_t image_flash_size;

    bm_dfu_event_t curr_evt = bm_dfu_get_current_event();

    /* Check if we even have a buf to inspect */
    if (! curr_evt.buf) {
        return;
    }

    bm_dfu_frame_t *frame = (bm_dfu_frame_t *) curr_evt.buf;
    bm_dfu_event_img_info_t* img_info_evt = (bm_dfu_event_img_info_t*) &((uint8_t *)frame)[1];

    image_size = img_info_evt->img_info.image_size;
    chunk_size = img_info_evt->img_info.chunk_size;
    minor_version = img_info_evt->img_info.minor_ver;
    major_version = img_info_evt->img_info.major_ver;


    memcpy((uint8_t *) &client_ctx.host_node_id, (uint8_t *) &img_info_evt->addresses.src_node_id, sizeof( img_info_evt->addresses.src_node_id));

    // Save image update info to noinit
    client_update_reboot_info.major = major_version;
    client_update_reboot_info.minor = minor_version;
    client_update_reboot_info.host_node_id = client_ctx.host_node_id;
    client_update_reboot_info.magic = DFU_REBOOT_MAGIC;

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
            bm_dfu_send_ack(client_ctx.host_node_id, 0, BM_DFU_ERR_FLASH_ACCESS);
            bm_dfu_client_transition_to_error(BM_DFU_ERR_FLASH_ACCESS);
        } else {

            if(client_ctx.fa->fa_size > image_size) {
                /* Erase memory in secondary image slot */
                printf("Erasing flash\n");

                image_flash_size = image_size;
                image_flash_size += (0x2000 - 1);
                image_flash_size &= ~(0x2000 - 1);

                if(flash_area_erase(client_ctx.fa, 0, image_flash_size) != 0) {
                    printf("Error erasing flash!\n");
                    bm_dfu_send_ack(client_ctx.host_node_id, 0, BM_DFU_ERR_FLASH_ACCESS);
                    bm_dfu_client_transition_to_error(BM_DFU_ERR_FLASH_ACCESS);
                } else {
                    bm_dfu_send_ack(client_ctx.host_node_id, 1, BM_DFU_ERR_NONE);

                    /* (zephyr) TODO: Fix this. Is this needed for FreeRTOS */
                    vTaskDelay(10); // Needed so ACK can properly be sent/processed
                    bm_dfu_set_pending_state_change(BM_DFU_STATE_CLIENT_RECEIVING);

                }
            } else {
                bm_dfu_send_ack(client_ctx.host_node_id, 0, BM_DFU_ERR_TOO_LARGE);
                bm_dfu_set_pending_state_change(BM_DFU_STATE_IDLE);

            }
        }
    } else {
        bm_dfu_send_ack(client_ctx.host_node_id, 0, BM_DFU_ERR_SAME_VER);
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
    bm_dfu_req_next_chunk(client_ctx.host_node_id, client_ctx.current_chunk);

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
        configASSERT(curr_evt.buf);
        bm_dfu_frame_t *frame = (bm_dfu_frame_t *) curr_evt.buf;
        bm_dfu_event_image_chunk_t* image_chunk_evt = (bm_dfu_event_image_chunk_t*) &((uint8_t *)frame)[1];

        /* Stop Chunk Timer */
        configASSERT(xTimerStop(client_ctx.chunk_timer, 10));

        /* Get Chunk Length and Chunk */
        client_ctx.chunk_length = image_chunk_evt->payload_length;

        /* Calculate Running CRC */
        client_ctx.running_crc16 = crc16_ccitt(client_ctx.running_crc16, image_chunk_evt->payload_buf, client_ctx.chunk_length);

        /* Process the frame */
        if (bm_dfu_process_payload(client_ctx.chunk_length, image_chunk_evt->payload_buf)) {
            bm_dfu_client_transition_to_error(BM_DFU_ERR_BM_FRAME);
        }

        /* Request Next Chunk */
        client_ctx.current_chunk++;
        client_ctx.chunk_retry_num = 0;

        if (client_ctx.current_chunk < client_ctx.num_chunks) {
            bm_dfu_req_next_chunk(client_ctx.host_node_id, client_ctx.current_chunk);
            configASSERT(xTimerStart(client_ctx.chunk_timer, 10));
        } else {
            /* Process the frame */
            if (bm_dfu_process_end()) {
                bm_dfu_client_transition_to_error(BM_DFU_ERR_BM_FRAME);
            } else {
                bm_dfu_set_pending_state_change(BM_DFU_STATE_CLIENT_VALIDATING);
            }
        }
    } else if (curr_evt.type == DFU_EVENT_CHUNK_TIMEOUT) {
        client_ctx.chunk_retry_num++;
        /* Try requesting chunk until max retries is reached */
        if (client_ctx.chunk_retry_num >= BM_DFU_MAX_CHUNK_RETRIES) {
            bm_dfu_client_abort();
            bm_dfu_client_transition_to_error(BM_DFU_ERR_TIMEOUT);
        } else {
            bm_dfu_req_next_chunk(client_ctx.host_node_id, client_ctx.current_chunk);
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
        printf("Rx Len: %" PRIu32 ", Actual Len: %" PRIu32 "\n", client_ctx.image_size, client_ctx.img_flash_offset);
        bm_dfu_update_end(client_ctx.host_node_id, 0, BM_DFU_ERR_MISMATCH_LEN);
        bm_dfu_client_transition_to_error(BM_DFU_ERR_MISMATCH_LEN);

    } else {
        /* Verify CRC. If ok, then move to Activating state */
        if (client_ctx.crc16 == client_ctx.running_crc16) {
            bm_dfu_set_pending_state_change(BM_DFU_STATE_CLIENT_REBOOT_REQ);
        } else {
            printf("Expected Image CRC: %d | Calculated Image CRC: %d\n", client_ctx.crc16, client_ctx.running_crc16);
            bm_dfu_update_end(client_ctx.host_node_id, 0, BM_DFU_ERR_BAD_CRC);
            bm_dfu_client_transition_to_error(BM_DFU_ERR_BAD_CRC);
        }
    }
}

/**
 * @brief Entry Function for the Client Activating State
 *
 * @note Upon confirmation to update recepit from the host, the device will set pending image bit and reboot
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
 * @brief Entry Function for the Client reboot request State
 *
 * @note Client will send the first request for reboot from the host and kickoff a Chunk timeout timer
 *
 * @return none
 */
void s_client_reboot_req_entry(void) {
    client_ctx.chunk_retry_num = 0;
    /* Request reboot */
    bm_dfu_client_send_reboot_request();

    /* Kickoff Chunk timeout */
    configASSERT(xTimerStart(client_ctx.chunk_timer, 10));
}

/**
 * @brief Run Function for the Client Reboot Request State
 *
 * @note Upon validation of received image, the device will request confirmation to reboot from the host.
 *
 * @return none
 */
void s_client_reboot_req_run(void) {
    bm_dfu_event_t curr_evt = bm_dfu_get_current_event();

    if (curr_evt.type == DFU_EVENT_REBOOT) {
        configASSERT(curr_evt.buf);
        configASSERT(xTimerStop(client_ctx.chunk_timer, 10));
        bm_dfu_set_pending_state_change(BM_DFU_STATE_CLIENT_ACTIVATING);
    } else if (curr_evt.type == DFU_EVENT_CHUNK_TIMEOUT) {
        client_ctx.chunk_retry_num++;
        /* Try requesting reboot until max retries is reached */
        if (client_ctx.chunk_retry_num >= BM_DFU_MAX_CHUNK_RETRIES) {
            bm_dfu_client_abort();
            bm_dfu_client_transition_to_error(BM_DFU_ERR_TIMEOUT);
        } else {
            bm_dfu_client_send_reboot_request();
            configASSERT(xTimerStart(client_ctx.chunk_timer, 10));
        }
    }
}

/**
 * @brief Entry Function for the Client Update Done
 *
 * @return none
 */
void s_client_update_done_entry(void) {
    client_ctx.chunk_retry_num = 0;
    if(getVersionInfo()->maj == client_update_reboot_info.major && getVersionInfo()->min == client_update_reboot_info.minor) {
        bm_dfu_client_send_boot_complete(client_update_reboot_info.host_node_id);
        /* Kickoff Chunk timeout */
        configASSERT(xTimerStart(client_ctx.chunk_timer, 10));
    } else {
        bm_dfu_update_end(client_update_reboot_info.host_node_id, false, BM_DFU_ERR_WRONG_VER);
        bm_dfu_client_transition_to_error(BM_DFU_ERR_WRONG_VER);
    }
}

/**
 * @brief Run Function for the Client Update Done
 *
 * @note After rebooting and confirming an update, client will let the host know that the update is complete.
 *
 * @return none
 */
void s_client_update_done_run(void) {
    bm_dfu_event_t curr_evt = bm_dfu_get_current_event();

    if (curr_evt.type == DFU_EVENT_UPDATE_END) {
        configASSERT(curr_evt.buf);
        configASSERT(xTimerStop(client_ctx.chunk_timer, 10));
        boot_set_confirmed();
        printf("Boot confirmed!\n Update success!\n");
        bm_dfu_update_end(client_update_reboot_info.host_node_id, true, BM_DFU_ERR_NONE);
        bm_dfu_set_pending_state_change(BM_DFU_STATE_IDLE);
    } else if (curr_evt.type == DFU_EVENT_CHUNK_TIMEOUT) {
        client_ctx.chunk_retry_num++;
        /* Try requesting confirmation until max retries is reached */
        if (client_ctx.chunk_retry_num >= BM_DFU_MAX_CHUNK_RETRIES) {
            bm_dfu_client_abort();
            bm_dfu_client_transition_to_error(BM_DFU_ERR_TIMEOUT);
        } else {
            /* Request confirmation */
            bm_dfu_client_send_boot_complete(client_update_reboot_info.host_node_id);
            configASSERT(xTimerStart(client_ctx.chunk_timer, 10));
        }
    }
}

/**
 * @brief Initialization function for the DFU Client subsystem
 *
 * @note Gets relevant message queues and semaphores, and creates Chunk Timeout Timer
 *
 * @param sock    Socket from DFU Core
 * @return none
 */

void bm_dfu_client_init(bcmp_dfu_tx_func_t bcmp_dfu_tx)
{
    configASSERT(bcmp_dfu_tx);
    client_ctx.bcmp_dfu_tx = bcmp_dfu_tx;
    int32_t tmr_id = 0;

    /* Store relevant variables */
    client_ctx.self_node_id = getNodeId();

    /* Get DFU Subsystem Queue */
    client_ctx.dfu_event_queue = bm_dfu_get_event_queue();

    client_ctx.chunk_timer = xTimerCreate("DFU Client Chunk Timer", (BM_DFU_CLIENT_CHUNK_TIMEOUT_MS / portTICK_RATE_MS),
                                      pdFALSE, (void *) &tmr_id, chunk_timer_handler);
    configASSERT(client_ctx.chunk_timer);
}

static void bm_dfu_client_transition_to_error(bm_dfu_err_t err) {
    configASSERT(xTimerStop(client_ctx.chunk_timer, 10));
    bm_dfu_set_error(err);
    bm_dfu_set_pending_state_change(BM_DFU_STATE_ERROR);
}
