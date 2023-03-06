#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "lwip/netif.h"
#include "lwip/pbuf.h"
#include "lwip/udp.h"

#ifdef __cplusplus
extern "C" {
#endif

// typedef void (*bm_pubsub_cb_t)(char* topic, char* data);

// typedef struct mw_cb_s {
//     sub_cb_t cb;
//     bm_pubsub_cb_t* next;
// } bm_pubsub_cb_t;

typedef struct bm_sub_s {
    char *topic;
    uint16_t topic_len;
    bool has_local_sub;
    bool has_payload_sub;
    //bm_pubsub_cb_t callback_list;
} bm_sub_t;

typedef struct bm_pub_s {
    char *topic;
    uint16_t topic_len;
    char *data;
    uint16_t data_len;
} bm_pub_t;

void bm_pubsub_init(struct netif* netif, struct udp_pcb* pcb, uint16_t port);
bool bm_pubsub_publish(bm_pub_t* pub);
bool bm_pubsub_subscribe(bm_sub_t* sub);
bool bm_pubsub_unsubscribe(bm_sub_t* sub);
void bm_pubsub_handle_msg(struct pbuf *pbuf, char* delim_ptr);
void bm_pubsub_print_subs(void);

#ifdef __cplusplus
}
#endif
