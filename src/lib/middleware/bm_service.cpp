#include "bm_service.h"
#include "bm_pubsub.h"
#include "FreeRTOS.h"
#include "string.h"
#include "semphr.h"
#include "bm_service_common.h"

static constexpr uint32_t DEFAULT_SERVICE_REQUEST_TIMEOUT_MS = 100;

typedef struct bm_service_list_elem {
    const char * service;
    size_t service_strlen;
    bm_service_handler service_handler;
    bm_service_list_elem * next;
} bm_service_list_elem_t;

typedef struct bm_service_context {
    bm_service_list_elem_t * service_list;
    SemaphoreHandle_t lock;
} bm_service_context_t;

static bm_service_context_t _bm_service_context;

static void _service_list_add_service(bm_service_list_elem_t * list_elem);
static bool _service_list_remove_service(const char * service, size_t service_strlen);
static bm_service_list_elem_t* _service_create_list_elem(size_t service_strlen, const char * service, bm_service_handler service_handler);
static bool _service_sub_unsub_to_req_topic(size_t service_strlen, const char * service, bool sub);
static void _service_request_received_cb (uint64_t node_id, const char* topic, uint16_t topic_len, const uint8_t* data, uint16_t data_len, uint8_t type, uint8_t version);

/*!
 * @brief Register a service.
 * @param[in] service_strlen The length of the service string.
 * @param[in] service The service string.
 * @param[in] service_handler The callback to call when a request is received.
 * @return True if the service was registered, false otherwise.
 */
bool bm_service_register(size_t service_strlen, const char * service, bm_service_handler service_handler) {
    configASSERT(service);
    configASSERT(service_handler);
    bool rval = false;
    if(xSemaphoreTake(_bm_service_context.lock, pdMS_TO_TICKS(DEFAULT_SERVICE_REQUEST_TIMEOUT_MS) == pdPASS)) {
        do {
            bm_service_list_elem_t* list_elem =  _service_create_list_elem(service_strlen, service, service_handler);
            if(!list_elem) {
                break;
            }
            _service_list_add_service(list_elem);
            if(!_service_sub_unsub_to_req_topic(service_strlen, service, true)) {
                break;
            }
            rval = true;
        } while (0);
        xSemaphoreGive(_bm_service_context.lock);
    }
    return rval;
}

/*!
 * @brief Unregister a service.
 * @param[in] service_strlen The length of the service string.
 * @param[in] service The service string.
 * @return True if the service was unregistered, false otherwise.
 */
bool bm_service_unregister(size_t service_strlen, const char * service) {
    configASSERT(service);
    bool rval = false;
    if(xSemaphoreTake(_bm_service_context.lock, pdMS_TO_TICKS(DEFAULT_SERVICE_REQUEST_TIMEOUT_MS) == pdPASS)) {
        do {
            if(!_service_sub_unsub_to_req_topic(service_strlen, service, false)) {
                break;
            }
            if(!_service_list_remove_service(service, service_strlen)) {
                break;
            }
            rval = true;
        } while(0);
        xSemaphoreGive(_bm_service_context.lock);
    }
    return rval;
}
/*!
 * @brief Initialize the service module.
 * @note Will initialize both the service request and service reply modules.
 */
void bm_service_init(void) {
    bm_service_request_init();
    _bm_service_context.lock = xSemaphoreCreateMutex();
}

static void _service_list_add_service(bm_service_list_elem_t * list_elem) {
    configASSERT(list_elem);
    if (_bm_service_context.service_list == NULL) {
        _bm_service_context.service_list = list_elem;
    } else {
        bm_service_list_elem_t * current = _bm_service_context.service_list;
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = list_elem;
    }
}

static bm_service_list_elem_t* _service_create_list_elem(size_t service_strlen, const char * service, bm_service_handler service_handler) {
    bm_service_list_elem_t * list_elem = static_cast<bm_service_list_elem_t*>(pvPortMalloc(sizeof(bm_service_list_elem_t)));
    configASSERT(list_elem);
    list_elem->service = service;
    list_elem->service_strlen = service_strlen;
    list_elem->service_handler = service_handler;
    list_elem->next = NULL;
    return list_elem;
}

static bool _service_sub_unsub_to_req_topic(size_t service_strlen, const char * service, bool sub) {
    bool rval = false;
    char * topic_str = NULL;
    do {
        size_t topic_strlen = service_strlen + strlen(BM_SERVICE_REQ_STR);
        topic_str = static_cast<char*>(pvPortMalloc(topic_strlen));
        configASSERT(topic_str);
        memcpy(topic_str, service, service_strlen);
        memcpy(topic_str + service_strlen, BM_SERVICE_REQ_STR, strlen(BM_SERVICE_REQ_STR));
        if(sub){
            if(!bm_sub_wl(topic_str, topic_strlen, _service_request_received_cb)) {
                break;
            }
        } else {
            if(!bm_unsub_wl(topic_str, topic_strlen, _service_request_received_cb)) {
                break;
            }
        }
        rval = true;
    } while (0);
    if (topic_str) {
        vPortFree(topic_str);
    }
    return rval;
}

static void _service_request_received_cb (uint64_t node_id, const char* topic, uint16_t topic_len, const uint8_t* data, uint16_t data_len, uint8_t type, uint8_t version) {
    (void) type;
    (void) version;

    if(xSemaphoreTake(_bm_service_context.lock, pdMS_TO_TICKS(DEFAULT_SERVICE_REQUEST_TIMEOUT_MS) == pdPASS)){
        bm_service_list_elem_t * current = _bm_service_context.service_list;
        while (current != NULL) {
            if (strncmp(current->service, topic, current->service_strlen) == 0) {
                bm_service_request_data_header_s * request_header = (bm_service_request_data_header_s*) data;
                if(data_len != sizeof(bm_service_request_data_header_s) + request_header->data_size) {
                    printf("Request data length does not match header.\n");
                    break;
                }
                if(topic_len != current->service_strlen + strlen(BM_SERVICE_REQ_STR)) {
                    printf("Topic length does not match service length.\n");
                    break;
                }
                // We found the service, make a reply buffer, and call the handler to create a reply.
                size_t reply_len = MAX_BM_SERVICE_DATA_SIZE - sizeof(bm_service_reply_data_header_s); // Max size of reply data
                uint8_t * reply_data = static_cast<uint8_t*>(pvPortMalloc(MAX_BM_SERVICE_DATA_SIZE));
                bm_service_reply_data_header_s * reply_header = (bm_service_reply_data_header_s*) reply_data;
                memset(reply_data,0, MAX_BM_SERVICE_DATA_SIZE);
                if(current->service_handler(current->service_strlen, current->service, request_header->data_size, request_header->data, reply_len, reply_header->data)){
                    reply_header->target_node_id = node_id;
                    reply_header->id = request_header->id;
                    reply_header->data_size = reply_len; // On return from the handler, reply_len is now the actual size of the reply data
                    
                    // Publish the reply
                    size_t pub_topic_len = current->service_strlen + strlen(BM_SERVICE_REP_STR);
                    char * pub_topic = static_cast<char*>(pvPortMalloc(pub_topic_len));
                    configASSERT(pub_topic);
                    memcpy(pub_topic, current->service, current->service_strlen);
                    memcpy(pub_topic + current->service_strlen, BM_SERVICE_REP_STR, strlen(BM_SERVICE_REP_STR));
                    bm_pub_wl(pub_topic, pub_topic_len, reply_data, reply_len + sizeof(bm_service_reply_data_header_s), 0);
                    vPortFree(pub_topic);
                }

                // Free the allocated memory
                vPortFree(reply_data);
                break;
            }
            current = current->next;
        }
        xSemaphoreGive(_bm_service_context.lock);
    }
}

static bool _service_list_remove_service(const char * service, size_t service_strlen) {
    configASSERT(service);
    bool rval = false;
    bm_service_list_elem_t * current = _bm_service_context.service_list;
    bm_service_list_elem_t * prev = NULL;
    while (current != NULL) {
        if (strncmp(current->service, service, service_strlen) == 0) {
            if (prev == NULL) {
                _bm_service_context.service_list = current->next;
            } else {
                prev->next = current->next;
            }
            vPortFree(current);
            rval = true;
            break;
        }
        prev = current;
        current = current->next;
    }
    return rval;
}
