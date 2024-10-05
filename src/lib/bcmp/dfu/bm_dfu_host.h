#pragma once

#include "bm_dfu.h"

// Includes for FreeRTOS
#include "FreeRTOS.h"
#include "queue.h"
#include "timers.h"
#include "semphr.h"
#include "nvmPartition.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define BM_DFU_MAX_ACK_RETRIES              2
#define BM_DFU_HOST_ACK_TIMEOUT_MS          10000UL
#define BM_DFU_HOST_HEARTBEAT_TIMEOUT_MS    1000UL
#define BM_DFU_UPDATE_DEFAULT_TIMEOUT_MS    (5 * 60 * 1000)

typedef int (*bm_dfu_chunk_req_cb)(uint16_t chunk_num, uint16_t *chunk_len, uint8_t *buf, uint16_t buf_len);

void s_host_req_update_entry(void);
void s_host_req_update_run(void);
void s_host_update_entry(void);
void s_host_update_run(void);

void bm_dfu_host_init(bcmp_dfu_tx_func_t bcmp_dfu_tx);
void bm_dfu_host_set_params(update_finish_cb_t update_complete_callback, uint32_t hostTimeoutMs);
bool bm_dfu_host_client_node_valid(uint64_t client_node_id);

#ifdef __cplusplus
}
#endif
