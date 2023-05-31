#include <string.h>
#include "FreeRTOS.h"
#include "lwip/ip_addr.h"
#include "lwip/inet.h"
#include "bm_pubsub.h"
#include "middleware.h"
#include "bm_util.h"

typedef struct {
  uint8_t type;
  uint8_t flags;
  uint8_t topic_len;
  const char topic[0];
} __attribute__((packed)) bm_header_t;

typedef struct {
    char *topic;
    uint16_t topic_len;
    bm_cb_t cb;
} bm_sub_t;

typedef struct bm_sub_node_s {
  bm_sub_t sub;
  struct bm_sub_node_s* next;
} bm_sub_node_t;

typedef struct {
  bm_sub_node_t* subscription_list;
} pubsubContext_t;

static bm_sub_node_t* get_sub(const char* topic, uint16_t topic_len);
static bm_sub_node_t* get_last_sub(void);
static pubsubContext_t _ctx;

extern const char* publication_topics;

/*!
  Subscribe to a specific string topic with callback

  \param[in] *topic topic string to subscribe to
  \param[in] callback callback function to call when data is received on this topic
  \return True if we've successfully subscribed
*/
bool bm_sub(const char *topic, bm_cb_t callback) {
  bool retv = false;

  uint16_t topic_len = strnlen(topic, BM_TOPIC_MAX_LEN);

  if(topic_len && (topic_len < BM_TOPIC_MAX_LEN)) {
    retv = bm_sub_wl(topic, topic_len, callback);
  }

  return retv;
}

/*!
  Subscribe to a specific string topic with callback (while providing topic_len)

  \param[in] *topic topic string to subscribe to
  \param[in] topic_len length of topic string
  \param[in] callback callback function to call when data is received on this topic
  \return True if we've successfully subscribed
*/
bool bm_sub_wl(const char *topic, uint16_t topic_len, bm_cb_t callback) {
  bool retv = true;

  do {
    // TODO - validate topic name if needed

    if(topic_len >= BM_TOPIC_MAX_LEN) {
      retv = false;
      // Invalid topic len
      break;
    }

    bm_sub_t sub = {
      .topic=pvPortMalloc(topic_len + 1),
      .topic_len=topic_len,
      .cb=callback
    };

    configASSERT(sub.topic);

    memcpy(sub.topic, topic, topic_len + 1);
    sub.topic[topic_len] = 0;

    if (_ctx.subscription_list) {
      bm_sub_node_t* ptr = get_sub(sub.topic, sub.topic_len);

      // Subscription already exists, add a new callback
      if (ptr) {
        ptr->sub.cb = sub.cb;
      /* Need to add new node to subscription list */
      } else {
        ptr = get_last_sub();
        ptr->next = (bm_sub_node_t *) pvPortMalloc(sizeof(bm_sub_node_t));
        if(!ptr->next){
          retv = false;
          break;
        }

        memset(ptr->next, 0x00, sizeof(bm_sub_node_t));
        ptr->next->sub.topic = (char *) pvPortMalloc(sub.topic_len);
        if(!ptr->next->sub.topic){
          vPortFree(ptr->next);
          retv = false;
          break;
        }
        memcpy(ptr->next->sub.topic, sub.topic, sub.topic_len);
        ptr->next->sub.topic_len = sub.topic_len;
        ptr->next->sub.cb = sub.cb;
      }
    /* Empty subscription list */
    } else {
      _ctx.subscription_list = (bm_sub_node_t *) pvPortMalloc(sizeof(bm_sub_node_t));
      if(!_ctx.subscription_list){
        retv = false;
        break;
      }
      _ctx.subscription_list->sub.topic = (char *) pvPortMalloc(sub.topic_len);
      if(!_ctx.subscription_list->sub.topic){
        vPortFree(_ctx.subscription_list);
        retv = false;
        break;
      }
      memcpy(_ctx.subscription_list->sub.topic, sub.topic, sub.topic_len);
      _ctx.subscription_list->sub.topic_len = sub.topic_len;
      _ctx.subscription_list->sub.cb = sub.cb;
      _ctx.subscription_list->next = NULL;
    }
  } while(0);

  if (retv) {
    printf("Subscribing to Topic: %s\n", topic);
  } else {
    printf("Unable to Subscribe to topic\n");
  }

  return retv;
}

/*!
  Unsubscribe callback from specific string topic

  \param[in] *topic topic string to unsubscribe from
  \param[in] callback callback function to unsubscribe from topic
  \return True if we've successfully subscribed
*/
bool bm_unsub(const char *topic, bm_cb_t callback) {
  bool retv = false;

  uint16_t topic_len = strnlen(topic, BM_TOPIC_MAX_LEN);

  if(topic_len && (topic_len < BM_TOPIC_MAX_LEN)) {
    retv = bm_unsub_wl(topic, topic_len, callback);
  }

  return retv;
}

/*!
  Unsubscribe callback from specific string topic (while providing topic len)

  \param[in] *topic topic string to unsubscribe from
  \param[in] topic_len length of topic string
  \param[in] callback callback function to unsubscribe from topic
  \return True if we've successfully subscribed
*/
bool bm_unsub_wl(const char *topic, uint16_t topic_len, bm_cb_t callback) {

  bool retv = false;

  do {
    if(topic_len >= BM_TOPIC_MAX_LEN) {
      // Invalid topic len
      break;
    }

    bm_sub_t sub = {
      .topic=(char *)topic,
      .topic_len=topic_len,
      .cb=callback
    };

    if (_ctx.subscription_list) {
      bm_sub_node_t* ptr = get_sub(sub.topic, topic_len);
      /* Subscription already exists, update */
      if (ptr) {
        ptr->sub.cb = NULL;
        retv = true;
      }
    }
  } while (0);

  if (retv) {
    printf("Unubscribed from Topic: %s\n", topic);
  } else {
    printf("Unable to Unsubscribe to topic\n");
  }

  return retv;
}

/*!
  Publish data to specific string topic

  \param[in] *topic topic string to unsubscribe from
  \param[in] *data pointer to data to publish
  \param[in] length of data to publish
  \return True if data has been queued to be publish (does not guarantee that it will be published though!)
*/
bool bm_pub(const char *topic, const void *data, uint16_t len) {
  bool retv = false;

  uint16_t topic_len = strnlen(topic, BM_TOPIC_MAX_LEN);

  if(topic_len && (topic_len < BM_TOPIC_MAX_LEN)) {
    retv = bm_pub_wl(topic, topic_len, data, len);
  }

  return retv;
}

/*!
  Publish data to specific string topic (while providing topic len)

  \param[in] *topic topic string to unsubscribe from
  \param[in] topic_len length of topic string
  \param[in] *data pointer to data to publish
  \param[in] length of data to publish
  \return True if data has been queued to be publish (does not guarantee that it will be published though!)
*/
bool bm_pub_wl(const char *topic, uint16_t topic_len, const void *data, uint16_t len) {
  bool retv = true;

  do {

    uint16_t message_size = sizeof(bm_header_t) + topic_len + len;
    struct pbuf *pbuf = pbuf_alloc(PBUF_TRANSPORT, message_size, PBUF_RAM);
    if(!pbuf) {
      retv = false;
      break;
    }

    bm_header_t *header = (bm_header_t *)pbuf->payload;
    // TODO actually set the type here
    header->type = 0;
    header->flags = 0;
    header->topic_len = topic_len;

    memcpy((void *)header->topic, topic, topic_len);
    memcpy((void *)&header->topic[header->topic_len], data, len);

    // If we have a local subscription, submit it to the local queue as well
    if (get_sub(topic, topic_len)) {
      // Submit to local queue as well. Function will pbuf_ref(pbuf) since it
      // will be used elsewhere
      bm_middleware_local_pub(pbuf);
    }

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
  \param[in] node_id - node id for sender
  \param[in] *pbuf - pbuf with incoming data
  \return None
*/
void bm_handle_msg(uint64_t node_id, struct pbuf *pbuf) {
  bm_header_t *header = (bm_header_t *)pbuf->payload;
  uint16_t data_len = pbuf->len - sizeof(bm_header_t) - header->topic_len;

  // TODO check header type and flags and do something about it

  bm_sub_node_t* ptr = get_sub(header->topic, header->topic_len);

  if (ptr && ptr->sub.cb) {
    ptr->sub.cb(node_id, header->topic, header->topic_len, (const uint8_t *)&header->topic[header->topic_len], data_len);
  }
}

/*!
  Print subscription linked list
  \param[in] *parameters - unused
  \return *bm_sub_node_t, last element in linked list
*/
void bm_print_subs(void) {
  bm_sub_node_t* node = _ctx.subscription_list;

  while(node != NULL) {
    printf("Node: %.*s\n", node->sub.topic_len, node->sub.topic);
    node = node->next;
  }
}

/*!
  Get all reported publications
  \return *char, string of pubs
*/
char* bm_get_pubs(void) {
  return (char *) publication_topics;
}

#define MAX_SUB_STR_LEN 256

/*!
  Get all registered subscriptions. Returned value must be freed by user
  \return *char, string of subs
*/
char* bm_get_subs(void) {
  bm_sub_node_t* node = _ctx.subscription_list;
  char* subs_string = (char *) pvPortMalloc(MAX_SUB_STR_LEN);
  memset(subs_string, 0, MAX_SUB_STR_LEN);
  configASSERT(subs_string);
  char* ptr = subs_string;
  char spacing[] = " | ";
  bool first = true;

  while(node != NULL) {
    if (first) {
      first = false;
    } else {
      strcat(ptr, spacing);
      ptr += sizeof(spacing) - 1;
    }
    strncat(ptr, node->sub.topic, node->sub.topic_len);
    ptr += node->sub.topic_len;

    node = node->next;
  }
  *ptr = 0x00; // Add Null terminator

  return subs_string;
}

/*!
  Finds a subscription in linked list based on topic
  \param[in] *topic - topic string
  \param[in] *topic_len - byte length of topic
  \return pointer to bm_sub_node_t, NULL if not found
*/
static bm_sub_node_t* get_sub(const char* topic, uint16_t topic_len) {
  bm_sub_node_t* node = _ctx.subscription_list;

  while(node != NULL) {
    if ((node->sub.topic_len == topic_len) &&
      (memcmp(node->sub.topic, topic, topic_len) == 0)) {
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
