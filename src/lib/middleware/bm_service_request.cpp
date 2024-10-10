#include "bm_service_request.h"

#include "FreeRTOS.h"
#include "timers.h"
extern "C" {
#include "timer_callback_handler.h"
}
#include "task.h"
#include "semphr.h"
#include "bm_pubsub.h"
#include <string.h>
#include "device_info.h"
#include "app_util.h"
#include "bm_service_common.h"

static constexpr uint32_t DEFAULT_SERVICE_REQUEST_TIMEOUT_MS = 100;
static constexpr uint32_t EXPIRY_TIMER_PERIOD_MS = 500;

typedef struct bm_service_request_node {
    char * service;
    size_t service_strlen;
    bm_service_reply_cb reply_cb;
    uint32_t timeout_ms;
    uint32_t request_start_ms;
    uint32_t id;
    bm_service_request_node * next;
} bm_service_request_node_t;

typedef struct bm_service_request_context {
    bm_service_request_node_t * service_request_list;
    uint32_t request_count;
    SemaphoreHandle_t lock;
    TimerHandle_t expiry_timer_handle;
} bm_service_request_context_t;

static bm_service_request_context_t _bm_service_request_context;

static bool _request_list_add_request(bm_service_request_node_t * node);
static bool _request_list_remove_request(bm_service_request_node_t * node);
static bm_service_request_node_t * _create_node(size_t service_strlen, const char * service, bm_service_reply_cb reply_cb, uint32_t timeout_ms);
static void _service_request_timer_callback(TimerHandle_t timer);
static bool _service_request_send_request(uint32_t msg_id, const char * service, size_t service_strlen, size_t data_len, const uint8_t * data);
static bool _service_request_sub_to_reply_topic(const char * service, size_t service_strlen);
static void _service_request_cb (uint64_t node_id, const char* topic, uint16_t topic_len, const uint8_t* data, uint16_t data_len, uint8_t type, uint8_t version);
static bm_service_request_node_t * _service_request_list_get_node_by_id(uint32_t id);
static void _service_request_timer_expiry_cb(void *arg);

/*!
 * @brief Initialize the service request module.
 * @note Must be called before any other functions in this module. Called by the bm_service module.
 */
void bm_service_request_init(void) {
    _bm_service_request_context.lock = xSemaphoreCreateMutex();
    _bm_service_request_context.expiry_timer_handle = xTimerCreate("Service request expiry timer", pdMS_TO_TICKS(EXPIRY_TIMER_PERIOD_MS), pdTRUE, NULL, _service_request_timer_callback);
    configASSERT(xTimerStart(_bm_service_request_context.expiry_timer_handle, 10) == pdPASS);
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
bool bm_service_request(size_t service_strlen, const char * service, size_t data_len, const uint8_t * data, bm_service_reply_cb reply_cb, uint32_t timeout_s) {
    bool rval = false;
    configASSERT(service);
    if(data_len){
        configASSERT(data);
    }
    bm_service_request_node_t * node = NULL;
    do {
        if(data_len > MAX_BM_SERVICE_DATA_SIZE) {
            printf("Data too large\n");
            break;
        }
        bm_service_request_node_t * node = _create_node(service_strlen, service, reply_cb, (timeout_s * 1000));
        configASSERT(node);
        if(!_request_list_add_request(node)) {
            printf("add request failed\n");
            break;
        }
        if(!_service_request_sub_to_reply_topic(service, service_strlen)) {
            printf("sub to reply topic failed\n");
            break;
        }
        if(!_service_request_send_request(node->id, service, service_strlen, data_len, data)) {
            printf("send request failed\n");
            break;
        }
        rval = true;
    } while(0);
    if(!rval) {
        if(node){
            if(node->service) {
                vPortFree(node->service);
            }
            vPortFree(node);
        }
    }
    return rval;
}

static bool _request_list_add_request(bm_service_request_node_t * node) {
    bool rval = false;
    configASSERT(node);
    if(xSemaphoreTake(_bm_service_request_context.lock, pdMS_TO_TICKS(DEFAULT_SERVICE_REQUEST_TIMEOUT_MS) == pdPASS)){
        if (_bm_service_request_context.service_request_list == NULL) {
            _bm_service_request_context.service_request_list = node;
        } else {
            bm_service_request_node_t * current = _bm_service_request_context.service_request_list;
            while (current->next != NULL) {
                current = current->next;
            }
            current->next = node;
        }
        rval = true;
        xSemaphoreGive(_bm_service_request_context.lock);
    }
    return rval;
}

static bool _request_list_remove_request(bm_service_request_node_t * node) {
    bool rval = false;
    configASSERT(node);
    bm_service_request_node_t * node_to_delete = NULL;
    if (_bm_service_request_context.service_request_list == node) {
        node_to_delete = _bm_service_request_context.service_request_list;
        _bm_service_request_context.service_request_list = node->next;
    } else {
        bm_service_request_node_t * current = _bm_service_request_context.service_request_list;
        while (current && current->next != node) {
            current = current->next;
        }
        if(current) {
            node_to_delete = current->next;
            current->next = node->next;
        }
    }
    if(node_to_delete){
        if(node_to_delete->service) {
            vPortFree(node_to_delete->service);
        }
        vPortFree(node_to_delete);
        rval = true;
    }
    return rval;
}

static bm_service_request_node_t * _create_node(size_t service_strlen, const char * service, bm_service_reply_cb reply_cb, uint32_t timeout_ms) {
    bm_service_request_node_t * node = static_cast<bm_service_request_node_t*>(pvPortMalloc(sizeof(bm_service_request_node_t)));
    configASSERT(node);
    node->service = static_cast<char*>(pvPortMalloc(service_strlen));
    configASSERT(node->service);
    memcpy(node->service, service, service_strlen);
    node->service_strlen = service_strlen;
    node->reply_cb = reply_cb;
    node->timeout_ms = timeout_ms;
    node->id = _bm_service_request_context.request_count++;
    node->next = NULL;
    node->request_start_ms = pdTICKS_TO_MS(xTaskGetTickCount());
    return node;
}

static void _service_request_timer_expiry_cb(void *arg) {
    (void) arg;
    if(xSemaphoreTake(_bm_service_request_context.lock, pdMS_TO_TICKS(DEFAULT_SERVICE_REQUEST_TIMEOUT_MS) == pdPASS)){
        bm_service_request_node_t * current = _bm_service_request_context.service_request_list;
        while(current) {
            if(!timeRemainingMs(current->request_start_ms, current->timeout_ms)) {
                printf("Expiring request id: %" PRIu32 "\n", current->id);
                if(current->reply_cb) {
                    current->reply_cb(false, current->id, current->service_strlen, current->service, 0, NULL);
                }
                _request_list_remove_request(current);
                current = _bm_service_request_context.service_request_list;
                continue;
            }
            current = current->next;
        }
        xSemaphoreGive(_bm_service_request_context.lock);
    }
}

static void _service_request_timer_callback(TimerHandle_t timer) {
    (void) timer;
    timer_callback_handler_send_cb(_service_request_timer_expiry_cb, timer, 0);
}

static bool _service_request_send_request(uint32_t msg_id, const char * service, size_t service_strlen, size_t data_len, const uint8_t * data) {
    bool rval = false;
    // Create string for request topic
    size_t request_str_len = service_strlen + strlen(BM_SERVICE_REQ_STR);
    char * request_str = static_cast<char*>(pvPortMalloc(request_str_len));
    configASSERT(request_str);
    memcpy(request_str, service, service_strlen);
    memcpy(request_str + service_strlen, BM_SERVICE_REQ_STR, strlen(BM_SERVICE_REQ_STR));

    // Create request packet and publish
    size_t req_len = data_len + sizeof(bm_service_request_data_header_s);
    uint8_t * req_data = static_cast<uint8_t*>(pvPortMalloc(req_len));
    configASSERT(req_data);
    bm_service_request_data_header_s * header = (bm_service_request_data_header_s*) req_data;
    header->id = msg_id;
    header->data_size = data_len;
    memcpy(header->data, data, data_len);
    rval = bm_pub_wl(request_str, request_str_len, req_data, req_len, 0);

    // Free memory
    vPortFree(request_str);
    vPortFree(req_data);
    return rval;
}

static bool _service_request_sub_to_reply_topic(const char * service, size_t service_strlen) {
    bool rval = false;
    size_t rep_str_len = service_strlen + strlen(BM_SERVICE_REP_STR);
    char * rep_str = static_cast<char*>(pvPortMalloc(rep_str_len));
    configASSERT(rep_str);
    memcpy(rep_str, service, service_strlen);
    memcpy(rep_str + service_strlen, BM_SERVICE_REP_STR, strlen(BM_SERVICE_REP_STR));
    rval = bm_sub_wl(rep_str, rep_str_len, _service_request_cb);
    vPortFree(rep_str);
    return rval;
}

static void _service_request_cb (uint64_t node_id, const char* topic, uint16_t topic_len, const uint8_t* data, uint16_t data_len, uint8_t type, uint8_t version) {
    (void) version;
    (void) type;
    (void) topic;
    (void) topic_len;
    (void) data_len;
    (void) node_id;
    bm_service_reply_data_header_s * header = (bm_service_reply_data_header_s*) data;
    if(header->target_node_id == getNodeId()){
        if(xSemaphoreTake(_bm_service_request_context.lock, pdMS_TO_TICKS(DEFAULT_SERVICE_REQUEST_TIMEOUT_MS) == pdPASS)) {
            bm_service_request_node_t * node = _service_request_list_get_node_by_id(header->id);
            if(node) {
                if(node->reply_cb) {
                    node->reply_cb(true, header->id, node->service_strlen, node->service, header->data_size, header->data);
                }
                _request_list_remove_request(node);
            }
            xSemaphoreGive(_bm_service_request_context.lock);
        }
    }
}

static bm_service_request_node_t * _service_request_list_get_node_by_id(uint32_t id) {
    bm_service_request_node_t * node = NULL;
    bm_service_request_node_t * current = _bm_service_request_context.service_request_list;
    while(current) {
        if(current->id == id) {
            node = current;
            break;
        }
        current = current->next;
    }
    return node;
}
