#include <string.h>
#include "FreeRTOS.h"
#include "lwip/ip_addr.h"
#include "lwip/inet.h"
#include "bm_pubsub.h"
#include "middleware.h"
#include "bm_util.h"

typedef struct bm_sub_node_s {
    bm_sub_t sub;
    struct bm_sub_node_s* next;
} bm_sub_node_t;

typedef struct {
    bm_sub_node_t* subscription_list;
} pubsubContext_t;

static bm_sub_node_t* get_sub(char* key, uint16_t key_len);
static bm_sub_node_t* get_last_sub(void);
static pubsubContext_t _ctx;

/*!
  Subscribe to a specific string topic
  \param[in] *bm_sub_t - struct with the subscription topic and scope of
  subscription (local or client)
  \return True if we've successfully subscribed
*/
bool bm_pubsub_subscribe(bm_sub_t* sub) {
    bool retv = true;

    do {
        /* Ensure that topic does not have illegal characters */
        if(strchr(sub->topic, ':')) {
            retv = false;
            break;
        }

        if (_ctx.subscription_list) {
            bm_sub_node_t* ptr = get_sub(sub->topic, sub->topic_len);
            /* Subscription already exists, update the callback */
            if (ptr) {
                ptr->sub.cb = sub->cb;
            /* Need to add new node to subscription list */
            } else {
                ptr = get_last_sub();
                ptr->next = (bm_sub_node_t *) pvPortMalloc(sizeof(bm_sub_node_t));
                if(!ptr->next){
                    retv = false;
                    break;
                }
                ptr->next->sub.topic = (char *) pvPortMalloc(sub->topic_len);
                if(!ptr->next->sub.topic){
                    vPortFree(ptr->next);
                    retv = false;
                    break;
                }
                memcpy(ptr->next->sub.topic, sub->topic, sub->topic_len);
                ptr->next->sub.topic_len = sub->topic_len;
                ptr->next->sub.cb = sub->cb;
            }
        /* Empty subscription list */
        } else {
            _ctx.subscription_list = (bm_sub_node_t *) pvPortMalloc(sizeof(bm_sub_node_t));
            if(!_ctx.subscription_list){
                retv = false;
                break;
            }
            _ctx.subscription_list->sub.topic = (char *) pvPortMalloc(sub->topic_len);
            if(!_ctx.subscription_list->sub.topic){
                vPortFree(_ctx.subscription_list);
                retv = false;
                break;
            }
            memcpy(_ctx.subscription_list->sub.topic, sub->topic, sub->topic_len);
            _ctx.subscription_list->sub.topic_len = sub->topic_len;
            _ctx.subscription_list->sub.cb = sub->cb;
        }
    } while(0);

    if (retv) {
        printf("Subscribing to Topic: %.*s\n", sub->topic_len, sub->topic);
    } else {
        printf("Unable to Subscribe to topic\n");
    }

    return retv;
}

/*!
  Unsubscribe to a specific string topic
  \param[in] *bm_sub_t - struct with the subscription topic and scope of
  subscription (local or client)
  \return True if we've successfully subscribed
*/
bool bm_pubsub_unsubscribe(bm_sub_t* sub) {
    bool retv = false;

    do {
        if (_ctx.subscription_list) {
            bm_sub_node_t* ptr = get_sub(sub->topic, sub->topic_len);
            /* Subscription already exists, update */
            if (ptr) {
                ptr->sub.cb = NULL;
                retv = true;
            }
        }
    } while (0);

    if (retv) {
        printf("Unubscribed from Topic: %.*s\n", sub->topic_len, sub->topic);
    } else {
        printf("Unable to Unsubscribe to topic\n");
    }

    return retv;
}

/*!
  Send data of a specific topic to all nodes in the network
  \param[in] *bm_pub_t - struct with the topic and associated data
  \return None
*/
bool bm_pubsub_publish(bm_pub_t* pub) {
    bool retv = true;

    do {
        /* Ensure that topic does not have illegal characters */
        if(strchr(pub->topic, ':')) {
            retv = false;
            break;
        }

        uint16_t message_size = pub->data_len + pub->topic_len + 1; // Add 1 for ':'
        struct pbuf *pbuf = pbuf_alloc(PBUF_TRANSPORT, message_size, PBUF_RAM);
        if(!pbuf) {
            retv = false;
            break;
        }

        memset(pbuf->payload, 0, message_size);
        memcpy(pbuf->payload, pub->topic, pub->topic_len);
        ((uint8_t *)pbuf->payload)[pub->topic_len] = ':';
        memcpy(&((uint8_t *)pbuf->payload)[pub->topic_len + 1], pub->data, pub->data_len);
        if (middleware_net_tx(pbuf)) {
            retv = false;
        }
        pbuf_free(pbuf);
    } while (0);

    if (!retv) {
        printf("Unable to publish to topic\n");
    }

    return retv;
}

/*!
  Handle incoming data that we are subscribed to.
  \param[in] *pbuf - pbuf with incoming data
  \param[in] *delim_ptr - address of delimiter in payload
  \return None
*/
void bm_pubsub_handle_msg(struct pbuf *pbuf, char* delim_ptr) {
    uint16_t topic_len = (delim_ptr - (char *)pbuf->payload);
    uint16_t data_len = pbuf->len - (delim_ptr - (char *) pbuf->payload) - 1;
    char * topic = (char *)pbuf->payload;
    char * data = delim_ptr + 1;
    bm_sub_node_t* ptr = get_sub((char *)pbuf->payload, topic_len);

    if (ptr && ptr->sub.cb) {
        ptr->sub.cb(topic, topic_len, data, data_len);
    }
}

/*!
  Print subscription linked list
  \param[in] *parameters - unused
  \return *bm_sub_node_t, last element in linked list
*/
void bm_pubsub_print_subs(void) {
    bm_sub_node_t* node = _ctx.subscription_list;

    while(node != NULL) {
        printf("Node: %.*s\n", node->sub.topic_len, node->sub.topic);
        node = node->next;
    }
}

/*!
  Finds a subscription in linked list based on key
  \param[in] *key - key string
  \param[in] *key_len - byte length of key
  \return pointer to bm_sub_node_t, NULL if not found
*/
static bm_sub_node_t* get_sub(char* key, uint16_t key_len) {
    bm_sub_node_t* node = _ctx.subscription_list;

    while(node != NULL) {
        if (memcmp(node->sub.topic, key, key_len) == 0) {
            break;
        }
        node = node->next;
    }
    return node;
}

/*!
  Get the last subscription in the linked list of subscription topics
  \param[in] *parameters - unused
  \return *bm_sub_node_t, last element in linked list
*/
static bm_sub_node_t* get_last_sub(void) {
    bm_sub_node_t* node = _ctx.subscription_list;
    bm_sub_node_t* last = node;

    while(node != NULL) {
        last = node;
        node = node->next;
    }
    return last;
}
