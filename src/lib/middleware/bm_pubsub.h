#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "lwip/netif.h"
#include "lwip/pbuf.h"
#include "lwip/udp.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BM_TOPIC_MAX_LEN (255)

typedef void (*bm_cb_t)(uint64_t node_id, const char* topic, uint16_t topic_len, const uint8_t* data, uint16_t data_len);

void bm_init(struct netif* netif, struct udp_pcb* pcb, uint16_t port);
bool bm_pub(const char *topic, const void *data, uint16_t len);
bool bm_pub_wl(const char *topic, uint16_t topic_len, const void *data, uint16_t len);
bool bm_sub(const char *topic, const bm_cb_t callback);
bool bm_sub_wl(const char *topic, uint16_t topic_len, const bm_cb_t callback);
bool bm_unsub(const char *topic, const bm_cb_t callback);
bool bm_unsub_wl(const char *topic, uint16_t topic_len, const bm_cb_t callback);
void bm_handle_msg(uint64_t node_id, struct pbuf *pbuf);
void bm_print_subs(void);
char* bm_get_pubs(void);
char* bm_get_subs(void);

#ifdef __cplusplus
}
#endif
