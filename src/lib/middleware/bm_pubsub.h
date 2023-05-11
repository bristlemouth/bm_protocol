#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "lwip/netif.h"
#include "lwip/pbuf.h"
#include "lwip/udp.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*bm_pubsub_cb_t)(uint64_t node_id, const char* topic, uint16_t topic_len, const uint8_t* data, uint16_t data_len);

typedef struct {
    char *topic;
    uint16_t topic_len;
    bm_pubsub_cb_t cb;
} bm_sub_t;

typedef struct {
    char *topic;
    uint16_t topic_len;
    char *data;
    uint16_t data_len;
} bm_pub_t;

void bm_pubsub_init(struct netif* netif, struct udp_pcb* pcb, uint16_t port);
bool bm_pubsub_publish(bm_pub_t* pub);
bool bm_pubsub_subscribe(bm_sub_t* sub);
bool bm_pubsub_unsubscribe(bm_sub_t* sub);
void bm_pubsub_handle_msg(uint64_t node_id, struct pbuf *pbuf);
void bm_pubsub_print_subs(void);
char* bm_pubsub_get_pubs(void);
char* bm_pubsub_get_subs(void);

#ifdef __cplusplus
}
#endif
