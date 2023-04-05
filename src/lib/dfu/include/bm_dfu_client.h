#pragma once

#include "bm_dfu.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define BM_DFU_CLIENT_CHUNK_TIMEOUT_MS  2000UL

void bm_dfu_client_process_request(void);

/* HFSM functions */
void s_client_run(void);
void s_client_receiving_entry(void);
void s_client_receiving_run(void);
void s_client_validating_entry(void);
void s_client_validating_run(void);
void s_client_activating_entry(void);
void s_client_activating_run(void);

void bm_dfu_client_init(ip6_addr_t _self_addr, struct udp_pcb* _pcb, uint16_t _port, struct netif* _netif);

#ifdef __cplusplus
}
#endif
