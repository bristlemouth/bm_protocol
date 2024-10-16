#include "bm_pubsub.h"
extern "C" {
#include "bm_ip.h"
#include "bm_os.h"
#include "messages/resource_discovery.h"
}
#include "middleware.h"
#include "util.h"
#include <string.h>

#define max_sub_str_len 256

typedef struct {
  uint8_t type;
  uint8_t flags;
  uint8_t topic_len;
  bm_common_pub_sub_header_t ext_header;
  const char topic[0];
} __attribute__((packed)) BmPubSubHeader;

// Used for callback linked-list
typedef struct BmPubSubNode {
  struct BmPubSubNode *next;
  BmPubSubCb callback_fn;
} BmPubSubNode;

typedef struct {
  char *topic;
  uint16_t topic_len;
  BmPubSubNode *callbacks;
} BmSub;

typedef struct BmSubNode {
  BmSub sub;
  struct BmSubNode *next;
} BmSubNode;

typedef struct {
  BmSubNode subscription_list;
} PubSubCtx;

static BmSubNode *delete_sub(const char *topic, uint16_t topic_len);
static BmSubNode *get_sub(const char *topic, uint16_t topic_len);
static BmSubNode *get_last_sub(void);
static PubSubCtx CTX;

/*!
  @brief Subscribe to a specific string topic with callback

  @param *topic topic string to subscribe to
  @param callback callback function to call when data is received on this topic

  @return true if we've successfully subscribed
  @return false if failure
*/
bool bm_sub(const char *topic, const BmPubSubCb callback) {
  bool retv = false;

  uint16_t topic_len = strnlen(topic, BM_TOPIC_MAX_LEN);

  if (topic_len && (topic_len < BM_TOPIC_MAX_LEN)) {
    retv = bm_sub_wl(topic, topic_len, callback);
  }

  return retv;
}

/*!
  @brief Subscribe to a specific string topic with callback (while providing topic_len)

  @param *topic topic string to subscribe to
  @param topic_len length of topic string
  @param callback callback function to call when data is received on this topic

  @return true if we've successfully subscribed
  @return false if failure
*/
bool bm_sub_wl(const char *topic, uint16_t topic_len, const BmPubSubCb callback) {
  bool retv = true;

  do {
    // TODO - validate topic name if needed

    if (!topic || !topic_len || !callback) {
      retv = false;
      break;
    }

    if (topic_len >= BM_TOPIC_MAX_LEN) {
      retv = false;
      // Invalid topic len
      break;
    }

    BmSubNode *ptr = get_sub(topic, topic_len);

    // Subscription already exists, add a new callback
    if (ptr) {

      BmPubSubNode *last_cb_node = ptr->sub.callbacks;

      // Go to last node (but stop if one already matches the requested callback)
      while ((ptr->sub.callbacks->callback_fn != callback) && last_cb_node->next) {
        last_cb_node = last_cb_node->next;
      }

      if (ptr->sub.callbacks->callback_fn == callback) {
        // Callback already subscribed to this topic!
        break;
      }

      // Add new callback item to linked-list
      BmPubSubNode *cb_node = (BmPubSubNode *)bm_malloc(sizeof(BmPubSubNode));

      if (cb_node) {
        cb_node->next = NULL;
        cb_node->callback_fn = callback;
        last_cb_node->next = cb_node;

        retv = true;
      }
    } else {
      // Creating new subscription from scratch

      ptr = get_last_sub();

      // If there's no subscriptions, point to the start of the list
      if (!ptr) {
        ptr = &CTX.subscription_list;
      }

      ptr->next = (BmSubNode *)bm_malloc(sizeof(BmSubNode));
      if (!ptr->next) {
        break;
      }

      memset(ptr->next, 0x00, sizeof(BmSubNode));
      ptr->next->sub.topic = (char *)bm_malloc(topic_len + 1);
      if (!ptr->next->sub.topic) {
        bm_free(ptr->next);
        break;
      }
      memcpy(ptr->next->sub.topic, topic, topic_len);
      ptr->next->sub.topic[topic_len] = 0;
      ptr->next->sub.topic_len = topic_len;

      // Add first callback item to linked-list
      BmPubSubNode *cb_node = (BmPubSubNode *)bm_malloc(sizeof(BmPubSubNode));

      if (cb_node) {
        cb_node->next = NULL;
        cb_node->callback_fn = callback;
        ptr->next->sub.callbacks = cb_node;

        retv = true;
      }
    }

  } while (0);

  if (retv) {
    printf("Subscribing to Topic: %.*s\n", topic_len, topic);
    if (bcmp_resource_discovery_add_resource(topic, topic_len, SUB,
                                             default_resource_add_timeout_ms) == BmOK) {
      printf("Added topic %.*s to BCMP resource table.\n", topic_len, topic);
    }
  } else {
    printf("Unable to Subscribe to topic\n");
  }

  return retv;
}

/*!
  @brief Unsubscribe callback from specific string topic

  @param *topic topic string to unsubscribe from
  @param callback callback function to unsubscribe from topic

  @return true if we've successfully subscribed
  @return false if failure
*/
bool bm_unsub(const char *topic, const BmPubSubCb callback) {
  bool retv = false;

  uint16_t topic_len = strnlen(topic, BM_TOPIC_MAX_LEN);

  if (topic_len && (topic_len < BM_TOPIC_MAX_LEN)) {
    retv = bm_unsub_wl(topic, topic_len, callback);
  }

  return retv;
}

/*!
  @brief Unsubscribe callback from specific string topic (while providing topic len)

  @param *topic topic string to unsubscribe from
  @param topic_len length of topic string
  @param callback callback function to unsubscribe from topic

  @return true if we've successfully unsubscribed
  @return false if failure
*/
bool bm_unsub_wl(const char *topic, uint16_t topic_len, const BmPubSubCb callback) {

  bool retv = false;

  do {
    if (topic_len >= BM_TOPIC_MAX_LEN) {
      // Invalid topic len
      break;
    }

    BmSubNode *ptr = get_sub(topic, topic_len);
    /* Subscription already exists, update */
    if (ptr) {

      BmPubSubNode *cb_node = ptr->sub.callbacks;
      BmPubSubNode *prev_node = NULL;

      // Check nodes for matching callback
      while ((cb_node->callback_fn != callback) && cb_node->next) {
        prev_node = cb_node;
        cb_node = cb_node->next;
      }

      // Didn't find a matching callback to unsubscribe :'(
      if (cb_node->callback_fn != callback) {
        break;
      }

      // Link to the next node in list
      if (prev_node) {
        prev_node->next = cb_node->next;
      } else {
        ptr->sub.callbacks = cb_node->next;
      }

      // Free deleted node
      bm_free(cb_node);

      // If there are no more callbacks, delete the sub entirely
      if (ptr->sub.callbacks == NULL) {
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

  @param *topic topic string to unsubscribe from
  @param *data pointer to data to publish
  @param length of data to publish
  @param type of data to publish
  @param version of data to publish

  @return true if data has been queued to be publish (does not guarantee that it will be published though!)
  @return false otherwise
*/
bool bm_pub(const char *topic, const void *data, uint16_t len, uint8_t type, uint8_t version) {
  bool retv = false;

  uint16_t topic_len = strnlen(topic, BM_TOPIC_MAX_LEN);

  if (topic_len && (topic_len < BM_TOPIC_MAX_LEN)) {
    retv = bm_pub_wl(topic, topic_len, data, len, type, version);
  }

  return retv;
}

/*!
  @brief Publish data to specific string topic (while providing topic len)

  @param[in] *topic topic string to unsubscribe from
  @param[in] topic_len length of topic string
  @param[in] *data pointer to data to publish
  @param[in] length of data to publish
  @param[in] type of data to publish
  @param[in] version of data to publish

  @return true if data has been queued to be publish (does not guarantee that it will be published though!)
  @return false otherwise
*/
bool bm_pub_wl(const char *topic, uint16_t topic_len, const void *data, uint16_t len,
               uint8_t type, uint8_t version) {
  bool retv = true;

  do {

    uint16_t message_size = sizeof(BmPubSubHeader) + topic_len + len;
    void *buf = bm_udp_new(message_size);
    if (!buf) {
      retv = false;
      break;
    }

    BmPubSubHeader *header = (BmPubSubHeader *)bm_udp_get_payload(buf);
    // TODO actually set the type here
    header->type = 0;
    header->flags = 0;
    header->topic_len = topic_len;
    header->ext_header.type = type;
    header->ext_header.version = version;

    memcpy((void *)header->topic, topic, topic_len);
    memcpy((void *)&header->topic[header->topic_len], data, len);

    // If we have a local subscription, submit it to the local queue as well
    if (get_sub(topic, topic_len)) {
      // Submit to local queue as well.
      // Caller must create a seperate buf than the IP stack send b/c
      // sending a buf to the IP stack must have a 1 reference count.
      // See: LWIP_IP_CHECK_PBUF_REF_COUNT_FOR_TX
      do {
        void *buf_local = bm_udp_new(message_size);
        if (!buf_local) {
          retv = false;
          break;
        }
        BmPubSubHeader *local_header = (BmPubSubHeader *)bm_udp_get_payload(buf_local);
        // TODO actually set the type here
        local_header->type = 0;
        local_header->flags = 0;
        local_header->topic_len = topic_len;
        local_header->ext_header.type = type;
        local_header->ext_header.version = version;

        memcpy((void *)local_header->topic, topic, topic_len);
        memcpy((void *)&local_header->topic[local_header->topic_len], data, len);
        // The reason why we push back to the middleware queue instead of running the callbacks here
        // is so they don't run in the current task context, which will depend on the caller.
        bm_middleware_local_pub(buf_local, message_size);
        bm_udp_cleanup(buf_local);
      } while (0);
    }

    if (bm_middleware_net_tx(buf, message_size)) {
      retv = false;
    }
    bm_udp_cleanup(buf);
  } while (0);

  if (!retv) {
    printf("Unable to publish to topic\n");
  } else {
    if (bcmp_resource_discovery_add_resource(topic, topic_len, PUB,
                                             default_resource_add_timeout_ms) == BmOK) {
      printf("Added topic %.*s to BCMP resource table.\n", topic_len, topic);
    }
  }

  return retv;
}

/*!
  @brief Handle incoming data that we are subscribed to

  @param node_id node id for sender
  @param *buf buf with incoming data
*/
void bm_handle_msg(uint64_t node_id, void *buf, uint32_t size) {
  BmPubSubHeader *header = (BmPubSubHeader *)bm_udp_get_payload(buf);
  uint16_t data_len = size - sizeof(BmPubSubHeader) - header->topic_len;

  // TODO check header type and flags and do something about it
  BmSubNode *ptr = get_sub(header->topic, header->topic_len);

  if (ptr && ptr->sub.callbacks) {
    BmPubSubNode *cb_node = ptr->sub.callbacks;

    while (cb_node) {
      cb_node->callback_fn(node_id, header->topic, header->topic_len,
                           (const uint8_t *)&header->topic[header->topic_len], data_len,
                           header->ext_header.type, header->ext_header.version);
      cb_node = cb_node->next;
    }
  }
}

/*!
  @brief Print subscription linked list
*/
void bm_print_subs(void) {
  BmSubNode *node = CTX.subscription_list.next;

  while (node != NULL) {
    // TODO, print number of callbacks subscribed
    printf("Node: %.*s\n", node->sub.topic_len, node->sub.topic);
    node = node->next;
  }
}

/*!
  @brief Get all registered subscriptions

  @details Returned value must be freed by user

  @return string of subs
*/
char *bm_get_subs(void) {
  BmSubNode *node = CTX.subscription_list.next;
  char *subs_string = (char *)bm_malloc(max_sub_str_len);

  if (subs_string) {
    char *ptr = subs_string;
    char spacing[] = " | ";
    bool first = true;

    memset(subs_string, 0, max_sub_str_len);
    while (node != NULL) {
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
  }

  return subs_string;
}

/*!
  @brief Deletes a subscription in linked list based on topic

  @param[in] *topic - topic string
  @param[in] *topic_len - byte length of topic

  @return pointer to BmSubNode
  @return NULL if not found
*/
static BmSubNode *delete_sub(const char *topic, uint16_t topic_len) {
  BmSubNode *node = CTX.subscription_list.next;
  BmSubNode *prev = &CTX.subscription_list;

  while (node != NULL) {
    if ((node->sub.topic_len == topic_len) &&
        (memcmp(node->sub.topic, topic, topic_len) == 0)) {

      // Delete node from list
      prev->next = node->next;

      // Free any callbacks that might have been linked
      BmPubSubNode *cb_node = node->sub.callbacks;
      while (cb_node) {
        BmPubSubNode *to_del = cb_node;
        cb_node = cb_node->next;

        // Free callback node
        bm_free(to_del);
      }

      bm_free(node->sub.topic);
      bm_free(node);
      break;
    }
    prev = node;
    node = node->next;
  }
  return node;
}

/*!
  @brief Finds a subscription in linked list based on topic

  @param *topic topic string
  @param topic_len byte length of topic

  @return pointer to BmSubNode
  @return NULL if not found
*/
static BmSubNode *get_sub(const char *topic, uint16_t topic_len) {
  BmSubNode *node = CTX.subscription_list.next;

  while (node != NULL) {
    if ((node->sub.topic_len == topic_len) &&
        (memcmp(node->sub.topic, topic, topic_len) == 0)) {
      break;
    }
    node = node->next;
  }
  return node;
}

/*!
  @brief Get the last subscription in the linked list of subscription topics

  @param[in] *parameters unused
  @return *BmSubNode, last element in linked list
*/
static BmSubNode *get_last_sub(void) {
  BmSubNode *node = CTX.subscription_list.next;
  BmSubNode *last = node;

  while (node != NULL) {
    last = node;
    node = node->next;
  }
  return last;
}
