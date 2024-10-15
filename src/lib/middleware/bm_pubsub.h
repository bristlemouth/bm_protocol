#pragma once

#include "bm_common_pub_sub.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BM_TOPIC_MAX_LEN (255)

typedef void (*BmPubSubCb)(uint64_t node_id, const char *topic, uint16_t topic_len,
                           const uint8_t *data, uint16_t data_len, uint8_t type,
                           uint8_t version);

void bm_init(struct netif *netif, struct udp_pcb *pcb, uint16_t port);
bool bm_pub(const char *topic, const void *data, uint16_t len, uint8_t type,
            uint8_t version = BM_COMMON_PUB_SUB_VERSION);
bool bm_pub_wl(const char *topic, uint16_t topic_len, const void *data, uint16_t len,
               uint8_t type, uint8_t version = BM_COMMON_PUB_SUB_VERSION);
bool bm_sub(const char *topic, const BmPubSubCb callback);
bool bm_sub_wl(const char *topic, uint16_t topic_len, const BmPubSubCb callback);
bool bm_unsub(const char *topic, const BmPubSubCb callback);
bool bm_unsub_wl(const char *topic, uint16_t topic_len, const BmPubSubCb callback);
void bm_handle_msg(uint64_t node_id, void *buf, uint32_t size);
void bm_print_subs(void);
char *bm_get_subs(void);

#ifdef __cplusplus
}
#endif
