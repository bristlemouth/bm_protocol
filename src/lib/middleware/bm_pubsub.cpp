#include <string.h>
#include "FreeRTOS.h"
#include "lwip/ip_addr.h"
#include "lwip/inet.h"
#include "bm_pubsub.h"
#include "middleware.h"
#include "bm_util.h"
#include "bcmp_resource_discovery.h"

typedef struct {
  uint8_t type;
  uint8_t flags;
  uint8_t topic_len;
  const char topic[0];
} __attribute__((packed)) bm_pubsub_header_t;

// Used for callback linked-list
typedef struct bm_cb_node_s {
  struct bm_cb_node_s *next;
  bm_cb_t callback_fn;
} bm_cb_node_t;

typedef struct {
    char *topic;
    uint16_t topic_len;
    bm_cb_node_t *callbacks;
} bm_sub_t;

typedef struct bm_sub_node_s {
  bm_sub_t sub;
  struct bm_sub_node_s* next;
} bm_sub_node_t;

typedef struct {
  bm_sub_node_t subscription_list;
} pubsubContext_t;

static bm_sub_node_t* delete_sub(const char* topic, uint16_t topic_len);
static bm_sub_node_t* get_sub(const char* topic, uint16_t topic_len);
static bm_sub_node_t* get_last_sub(void);
static pubsubContext_t _ctx;

/*!
  Subscribe to a specific string topic with callback

  \param[in] *topic topic string to subscribe to
  \param[in] callback callback function to call when data is received on this topic
  \return True if we've successfully subscribed
*/
bool bm_sub(const char *topic, const bm_cb_t callback) {
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
bool bm_sub_wl(const char *topic, uint16_t topic_len, const bm_cb_t callback) {
  bool retv = true;

  do {
    // TODO - validate topic name if needed

    if(!topic || !topic_len || !callback) {
      retv = false;
      break;
    }

    if(topic_len >= BM_TOPIC_MAX_LEN) {
      retv = false;
      // Invalid topic len
      break;
    }

    bm_sub_node_t* ptr = get_sub(topic, topic_len);

    // Subscription already exists, add a new callback
    if (ptr) {

      bm_cb_node_t *last_cb_node = ptr->sub.callbacks;

      // Go to last node (but stop if one already matches the requested callback)
      while((ptr->sub.callbacks->callback_fn != callback) && last_cb_node->next) {
        last_cb_node = last_cb_node->next;
      }

      if(ptr->sub.callbacks->callback_fn == callback) {
        // Callback already subscribed to this topic!
        break;
      }

      // Add new callback item to linked-list
      bm_cb_node_t *cb_node = (bm_cb_node_t *)pvPortMalloc(sizeof(bm_cb_node_t));
      configASSERT(cb_node);

      cb_node->next = NULL;
      cb_node->callback_fn = callback;
      last_cb_node->next = cb_node;

      retv = true;
    } else {
      // Creating new subscription from scratch

      ptr = get_last_sub();

      // If there's no subscriptions, point to the start of the list
      if(!ptr) {
        ptr = &_ctx.subscription_list;
      }

      ptr->next = (bm_sub_node_t *) pvPortMalloc(sizeof(bm_sub_node_t));
      if(!ptr->next){
        break;
      }

      memset(ptr->next, 0x00, sizeof(bm_sub_node_t));
      ptr->next->sub.topic = (char *)pvPortMalloc(topic_len + 1);
      if(!ptr->next->sub.topic){
        vPortFree(ptr->next);
        break;
      }
      memcpy(ptr->next->sub.topic, topic, topic_len);
      ptr->next->sub.topic[topic_len] = 0;
      ptr->next->sub.topic_len = topic_len;

      // Add first callback item to linked-list
      bm_cb_node_t *cb_node = (bm_cb_node_t *)pvPortMalloc(sizeof(bm_cb_node_t));
      configASSERT(cb_node);

      cb_node->next = NULL;
      cb_node->callback_fn = callback;
      ptr->next->sub.callbacks = cb_node;

      retv = true;
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
bool bm_unsub(const char *topic, const bm_cb_t callback) {
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
bool bm_unsub_wl(const char *topic, uint16_t topic_len, const bm_cb_t callback) {

  bool retv = false;

  do {
    if(topic_len >= BM_TOPIC_MAX_LEN) {
      // Invalid topic len
      break;
    }

    bm_sub_node_t* ptr = get_sub(topic, topic_len);
    /* Subscription already exists, update */
    if (ptr) {

      bm_cb_node_t *cb_node = ptr->sub.callbacks;
      bm_cb_node_t *prev_node = NULL;

      // Check nodes for matching callback
      while((cb_node->callback_fn != callback) && cb_node->next) {
        prev_node = cb_node;
        cb_node = cb_node->next;
      }

      // Didn't find a matching callback to unsubscribe :'(
      if(cb_node->callback_fn != callback) {
        break;
      }

      // Link to the next node in list
      if(prev_node) {
        prev_node->next = cb_node->next;
      } else {
        ptr->sub.callbacks = cb_node->next;
      }

      // Free deleted node
      vPortFree(cb_node);

      // If there are no more callbacks, delete the sub entirely
      if(ptr->sub.callbacks == NULL) {
        delete_sub(topic, topic_len);
      }

      retv = true;
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

    uint16_t message_size = sizeof(bm_pubsub_header_t) + topic_len + len;
    struct pbuf *pbuf = pbuf_alloc(PBUF_TRANSPORT, message_size, PBUF_RAM);
    if(!pbuf) {
      retv = false;
      break;
    }

    bm_pubsub_header_t *header = (bm_pubsub_header_t *)pbuf->payload;
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

      // The reason why we push back to the middleware queue instead of running the callbacks here
      // is so they don't run in the current task context, which will depend on the caller.
      bm_middleware_local_pub(pbuf);
    }

    if (middleware_net_tx(pbuf)) {
      retv = false;
    }
    pbuf_free(pbuf);
  } while (0);

  if (!retv) {
    printf("Unable to publish to topic\n");
  } else {
    if(bcmp_resource_discovery::bcmp_resource_discovery_add_resource(topic, topic_len)){
      printf("Added topic %.*s to BCMP resource table.\n",topic_len,topic);
    }
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
  bm_pubsub_header_t *header = (bm_pubsub_header_t *)pbuf->payload;
  uint16_t data_len = pbuf->len - sizeof(bm_pubsub_header_t) - header->topic_len;

  // TODO check header type and flags and do something about it

  bm_sub_node_t* ptr = get_sub(header->topic, header->topic_len);

  if (ptr && ptr->sub.callbacks) {
    bm_cb_node_t *cb_node = ptr->sub.callbacks;

    while(cb_node) {
      cb_node->callback_fn( node_id,
                            header->topic,
                            header->topic_len,
                            (const uint8_t *)&header->topic[header->topic_len],
                            data_len);
      cb_node = cb_node->next;
    }
  }
}

/*!
  Print subscription linked list
  \param[in] *parameters - unused
  \return *bm_sub_node_t, last element in linked list
*/
void bm_print_subs(void) {
  bm_sub_node_t* node = _ctx.subscription_list.next;

  while(node != NULL) {
    // TODO, print number of callbacks subscribed
    printf("Node: %.*s\n", node->sub.topic_len, node->sub.topic);
    node = node->next;
  }
}

#define MAX_SUB_STR_LEN 256

/*!
  Get all registered subscriptions. Returned value must be freed by user
  \return *char, string of subs
*/
char* bm_get_subs(void) {
  bm_sub_node_t* node = _ctx.subscription_list.next;
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
  Deletes a subscription in linked list based on topic
  \param[in] *topic - topic string
  \param[in] *topic_len - byte length of topic
  \return pointer to bm_sub_node_t, NULL if not found
*/
static bm_sub_node_t* delete_sub(const char* topic, uint16_t topic_len) {
  bm_sub_node_t* node = _ctx.subscription_list.next;
  bm_sub_node_t* prev = &_ctx.subscription_list;

  while(node != NULL) {
    if ((node->sub.topic_len == topic_len) &&
      (memcmp(node->sub.topic, topic, topic_len) == 0)) {

      // Delete node from list
      prev->next = node->next;

      // Free any callbacks that might have been linked
      bm_cb_node_t *cb_node = node->sub.callbacks;
      while(cb_node) {
        bm_cb_node_t *to_del = cb_node;
        cb_node = cb_node->next;

        // Free callback node
        vPortFree(to_del);
      }

      vPortFree(node->sub.topic);
      vPortFree(node);
      break;
    }
    prev = node;
    node = node->next;
  }
  return node;
}

/*!
  Finds a subscription in linked list based on topic
  \param[in] *topic - topic string
  \param[in] *topic_len - byte length of topic
  \return pointer to bm_sub_node_t, NULL if not found
*/
static bm_sub_node_t* get_sub(const char* topic, uint16_t topic_len) {
  bm_sub_node_t* node = _ctx.subscription_list.next;

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
  bm_sub_node_t* node = _ctx.subscription_list.next;
  bm_sub_node_t* last = node;

  while(node != NULL) {
    last = node;
    node = node->next;
  }
  return last;
}
