#include "bm_service_request.h"

#include "bm_service_common.h"
#include "pubsub.h"
#include "timer_callback_handler.h"
#include <inttypes.h>
#include <string.h>

// extern "C" {
#include "bm_os.h"
#include "device.h"
#include "util.h"
// }

#define DefaultServiceRequestTimeoutMs 100
#define ExpiryTimerPeriodMs 500

typedef struct BmServiceRequestNode {
  char *service;
  size_t service_strlen;
  BmServiceReplyCb reply_cb;
  uint32_t timeout_ms;
  uint32_t request_start_ms;
  uint32_t id;
  struct BmServiceRequestNode *next;
} BmServiceRequestNode;

typedef struct bm_service_request_context {
  BmServiceRequestNode *service_request_list;
  uint32_t request_count;
  BmSemaphore lock;
  BmTimer expiry_timer_handle;
} BmServiceRequestContext;

static BmServiceRequestContext _bm_service_request_context;

static bool _request_list_add_request(BmServiceRequestNode *node);
static bool _request_list_remove_request(BmServiceRequestNode *node);
static BmServiceRequestNode *_create_node(size_t service_strlen, const char *service,
                                          BmServiceReplyCb reply_cb, uint32_t timeout_ms);
static void _service_request_timer_callback(BmTimer timer);
static bool _service_request_send_request(uint32_t msg_id, const char *service,
                                          size_t service_strlen, size_t data_len,
                                          const uint8_t *data);
static bool _service_request_sub_to_reply_topic(const char *service, size_t service_strlen);
static void _service_request_cb(uint64_t node, const char *topic, uint16_t topic_len,
                                const uint8_t *data, uint16_t data_len, uint8_t type,
                                uint8_t version);
static BmServiceRequestNode *_service_request_list_get_node_by_id(uint32_t id);
static void _service_request_timer_expiry_cb(void *arg);

/*!
 * @brief Initialize the service request module.
 * @note Must be called before any other functions in this module. Called by the bm_service module.
 */
void bm_service_request_init(void) {
  _bm_service_request_context.lock = bm_semaphore_create();
  _bm_service_request_context.expiry_timer_handle =
      bm_timer_create("Service request expiry timer", bm_ms_to_ticks(ExpiryTimerPeriodMs), true,
                      NULL, _service_request_timer_callback);
  // TODO - make this return an error when the timer doesn't start!
  // configASSERT(bm_timer_start(_bm_service_request_context.expiry_timer_handle, 10) == BmOK);
  bm_timer_start(_bm_service_request_context.expiry_timer_handle, 10);
}

/*!
 * @brief Send a service request.
 * @param[in] service_strlen The length of the service string.
 * @param[in] service The service string.
 * @param[in] data_len The length of the data.
 * @param[in] data The data.
 * @param[in] reply_cb The callback to call when a reply is received.
 * @param[in] timeout_s The timeout in seconds.
 * @return True if the request was sent, false otherwise.
 */
bool bm_service_request(size_t service_strlen, const char *service, size_t data_len,
                        const uint8_t *data, BmServiceReplyCb reply_cb, uint32_t timeout_s) {
  bool rval = false;
  if (!service) {
    return rval;
  }
  if (!data_len || !data) {
    return rval;
  }
  BmServiceRequestNode *node = NULL;
  do {
    if (data_len > MAX_BM_SERVICE_DATA_SIZE) {
      printf("Data too large\n");
      break;
    }
    BmServiceRequestNode *node =
        _create_node(service_strlen, service, reply_cb, (timeout_s * 1000));
    if (!node) {
      printf("create node failed\n");
      break;
    }
    if (!_request_list_add_request(node)) {
      printf("add request failed\n");
      break;
    }
    if (!_service_request_sub_to_reply_topic(service, service_strlen)) {
      printf("sub to reply topic failed\n");
      break;
    }
    if (!_service_request_send_request(node->id, service, service_strlen, data_len, data)) {
      printf("send request failed\n");
      break;
    }
    rval = true;
  } while (0);
  if (!rval) {
    if (node) {
      if (node->service) {
        bm_free(node->service);
      }
      bm_free(node);
    }
  }
  return rval;
}

static bool _request_list_add_request(BmServiceRequestNode *node) {
  bool rval = false;
  if (!node) {
    return rval;
  }
  if (bm_semaphore_take(_bm_service_request_context.lock, DefaultServiceRequestTimeoutMs) ==
      BmOK) {
    if (_bm_service_request_context.service_request_list == NULL) {
      _bm_service_request_context.service_request_list = node;
    } else {
      BmServiceRequestNode *current = _bm_service_request_context.service_request_list;
      while (current->next != NULL) {
        current = current->next;
      }
      current->next = node;
    }
    rval = true;
    bm_semaphore_give(_bm_service_request_context.lock);
  }
  return rval;
}

static bool _request_list_remove_request(BmServiceRequestNode *node) {
  bool rval = false;
  if (!node) {
    return rval;
  }
  BmServiceRequestNode *node_to_delete = NULL;
  if (_bm_service_request_context.service_request_list == node) {
    node_to_delete = _bm_service_request_context.service_request_list;
    _bm_service_request_context.service_request_list = node->next;
  } else {
    BmServiceRequestNode *current = _bm_service_request_context.service_request_list;
    while (current && current->next != node) {
      current = current->next;
    }
    if (current) {
      node_to_delete = current->next;
      current->next = node->next;
    }
  }
  if (node_to_delete) {
    if (node_to_delete->service) {
      bm_free(node_to_delete->service);
    }
    bm_free(node_to_delete);
    rval = true;
  }
  return rval;
}

static BmServiceRequestNode *_create_node(size_t service_strlen, const char *service,
                                          BmServiceReplyCb reply_cb, uint32_t timeout_ms) {
  BmServiceRequestNode *node =
      (BmServiceRequestNode *)(bm_malloc(sizeof(BmServiceRequestNode)));
  if (!node) {
    return NULL;
  }
  node->service = (char *)(bm_malloc(service_strlen));
  if (!node->service) {
    bm_free(node);
    return NULL;
  }
  memcpy(node->service, service, service_strlen);
  node->service_strlen = service_strlen;
  node->reply_cb = reply_cb;
  node->timeout_ms = timeout_ms;
  node->id = _bm_service_request_context.request_count++;
  node->next = NULL;
  node->request_start_ms = bm_ticks_to_ms(bm_get_tick_count());
  return node;
}

static void _service_request_timer_expiry_cb(void *arg) {
  (void)arg;
  if (bm_semaphore_take(_bm_service_request_context.lock, DefaultServiceRequestTimeoutMs) ==
      BmOK) {
    BmServiceRequestNode *current = _bm_service_request_context.service_request_list;
    while (current) {
      if (!time_remaining_ms(current->request_start_ms, current->timeout_ms)) {
        printf("Expiring request id: %" PRIu32 "\n", current->id);
        if (current->reply_cb) {
          current->reply_cb(false, current->id, current->service_strlen, current->service, 0,
                            NULL);
        }
        _request_list_remove_request(current);
        current = _bm_service_request_context.service_request_list;
        continue;
      }
      current = current->next;
    }
    bm_semaphore_give(_bm_service_request_context.lock);
  }
}

static void _service_request_timer_callback(BmTimer timer) {
  (void)timer;
  timer_callback_handler_send_cb(_service_request_timer_expiry_cb, timer, 0);
}

static bool _service_request_send_request(uint32_t msg_id, const char *service,
                                          size_t service_strlen, size_t data_len,
                                          const uint8_t *data) {
  bool rval = false;
  // Create string for request topic
  size_t request_str_len = service_strlen + strlen(BM_SERVICE_REQ_STR);
  char *request_str = (char *)bm_malloc(request_str_len);
  if (!request_str) {
    return rval;
  }
  memcpy(request_str, service, service_strlen);
  memcpy(request_str + service_strlen, BM_SERVICE_REQ_STR, strlen(BM_SERVICE_REQ_STR));

  // Create request packet and publish
  size_t req_len = data_len + sizeof(BmServiceRequestDataHeader);
  uint8_t *req_data = (uint8_t *)(bm_malloc(req_len));
  if (!req_data) {
    bm_free(request_str);
    return rval;
  }
  BmServiceRequestDataHeader *header = (BmServiceRequestDataHeader *)req_data;
  header->id = msg_id;
  header->data_size = data_len;
  memcpy(header->data, data, data_len);
  rval =
      bm_pub_wl(request_str, request_str_len, req_data, req_len, 0, BM_COMMON_PUB_SUB_VERSION);

  // Free memory
  bm_free(request_str);
  bm_free(req_data);
  return rval;
}

static bool _service_request_sub_to_reply_topic(const char *service, size_t service_strlen) {
  bool rval = false;
  size_t rep_str_len = service_strlen + strlen(BM_SERVICE_REP_STR);
  char *rep_str = (char *)(bm_malloc(rep_str_len));
  if (!rep_str) {
    return rval;
  }
  memcpy(rep_str, service, service_strlen);
  memcpy(rep_str + service_strlen, BM_SERVICE_REP_STR, strlen(BM_SERVICE_REP_STR));
  rval = bm_sub_wl(rep_str, rep_str_len, _service_request_cb);
  bm_free(rep_str);
  return rval;
}

static void _service_request_cb(uint64_t node, const char *topic, uint16_t topic_len,
                                const uint8_t *data, uint16_t data_len, uint8_t type,
                                uint8_t version) {
  (void)version;
  (void)type;
  (void)topic;
  (void)topic_len;
  (void)data_len;
  (void)node;
  BmServiceReplyDataHeader *header = (BmServiceReplyDataHeader *)data;
  if (header->target_node_id == node_id()) {
    if (bm_semaphore_take(_bm_service_request_context.lock, DefaultServiceRequestTimeoutMs) ==
        BmOK) {
      BmServiceRequestNode *node = _service_request_list_get_node_by_id(header->id);
      if (node) {
        if (node->reply_cb) {
          node->reply_cb(true, header->id, node->service_strlen, node->service,
                         header->data_size, header->data);
        }
        _request_list_remove_request(node);
      }
      bm_semaphore_give(_bm_service_request_context.lock);
    }
  }
}

static BmServiceRequestNode *_service_request_list_get_node_by_id(uint32_t id) {
  BmServiceRequestNode *node = NULL;
  BmServiceRequestNode *current = _bm_service_request_context.service_request_list;
  while (current) {
    if (current->id == id) {
      node = current;
      break;
    }
    current = current->next;
  }
  return node;
}
