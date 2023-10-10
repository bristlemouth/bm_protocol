#include "bcmp_resource_discovery.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "string.h"
#include <stdlib.h>
#include "device_info.h"
#include "bcmp.h"
#include "bcmp_linked_list_generic.h"

using namespace bcmp_resource_discovery;

typedef struct bcmp_resource_node_t {
    bcmp_resource_t * resource;
    bcmp_resource_node_t * next;
} bcmp_resource_node_t;

typedef struct bcmp_resource_list_t {
    bcmp_resource_node_t* start;
    bcmp_resource_node_t* end;
    uint16_t num_resources;
    SemaphoreHandle_t lock;
} bcmp_resource_list_t;

static bcmp_resource_list_t _pub_list;
static bcmp_resource_list_t _sub_list;

static BCMP_Linked_List_Generic _resource_request_list;

static bool _bcmp_resource_discovery_find_resource(const char * resource, const uint16_t resource_len, resource_type_e type);
static bool _bcmp_resource_compute_list_size(resource_type_e type, size_t &msg_len);
static bool _bcmp_resource_populate_msg_data(resource_type_e type, bcmp_resource_table_reply_t * repl, uint32_t &data_offset);

static bool _bcmp_resource_compute_list_size(resource_type_e type, size_t &msg_len) {
    bool rval = false;
    bcmp_resource_list_t *res_list = (type == SUB) ? &_sub_list : &_pub_list;
    if(xSemaphoreTake(res_list->lock, pdMS_TO_TICKS(DEFAULT_RESOURCE_ADD_TIMEOUT_MS)) == pdPASS) {
        do {
            bcmp_resource_node_t * cur = res_list->start;
            while(cur != NULL) {
                msg_len += (sizeof(bcmp_resource_t) + cur->resource->resource_len);
                cur = cur->next;
            }
            rval = true;
        } while(0);
        xSemaphoreGive(res_list->lock);
    }
    return rval;
}

static bool _bcmp_resource_populate_msg_data(resource_type_e type, bcmp_resource_table_reply_t * repl, uint32_t &data_offset) {
    bool rval = false;
    bcmp_resource_list_t *res_list = (type == SUB) ? &_sub_list : &_pub_list;
    if(xSemaphoreTake(res_list->lock, pdMS_TO_TICKS(DEFAULT_RESOURCE_ADD_TIMEOUT_MS)) == pdPASS) {
        do {
            if(type == PUB){
                repl->num_pubs = res_list->num_resources;
            } else { // SUB
                repl->num_subs = res_list->num_resources;
            }
            bcmp_resource_node_t *cur = res_list->start;
            while(cur != NULL) {
                size_t res_size = (sizeof(bcmp_resource_t) + cur->resource->resource_len);
                memcpy(&repl->resource_list[data_offset], cur->resource, res_size);
                data_offset += res_size;
                cur = cur->next;
            }
            rval = true;
        } while(0);
        xSemaphoreGive(res_list->lock);
    }
    return rval;
}


/*!
  Process the resource discovery request message.

  \param in *req - request
  \param in *dst - destination IP to reply to.
  \return - None
*/
void bcmp_resource_discovery::bcmp_process_resource_discovery_request(bcmp_resource_table_request_t *req, const ip_addr_t *dst) {
    do {
        if(req->target_node_id != getNodeId()){
            break;
        }
        size_t msg_len = sizeof(bcmp_resource_table_reply_t);
        if(!_bcmp_resource_compute_list_size(PUB, msg_len)) {
            printf("Failed to get publishers list\n.");
            break;
        }
        if(!_bcmp_resource_compute_list_size(SUB, msg_len)) {
            printf("Failed to get subscribers list\n.");
            break;
        }
        // Create and fill the reply
        uint8_t * reply_buf = static_cast<uint8_t*>(pvPortMalloc(msg_len));
        configASSERT(reply_buf);
        do {
            bcmp_resource_table_reply_t * repl = reinterpret_cast<bcmp_resource_table_reply_t *>(reply_buf);
            repl->node_id = getNodeId();
            uint32_t data_offset = 0;
            if(!_bcmp_resource_populate_msg_data(PUB, repl, data_offset)) {
                printf("Failed to get publishers list\n.");
                break;
            }
            if(!_bcmp_resource_populate_msg_data(SUB, repl, data_offset)) {
                printf("Failed to get publishers list\n.");
                break;
            }
            if(bcmp_tx(dst, BCMP_RESOURCE_TABLE_REPLY, reply_buf, msg_len) != ERR_OK){
                printf("Failed to send bcmp resource table reply\n");
            }
        } while(0);
        vPortFree(reply_buf);
    } while(0);
}

/*!
  Process the resource discovery reply message.

  \param in *repl - reply
  \param in src_node_id - node ID of the source.
  \return - None
*/
void bcmp_resource_discovery::bcmp_process_resource_discovery_reply(bcmp_resource_table_reply_t *repl, uint64_t src_node_id) {
    do {
        if(repl->node_id != src_node_id){
            break;
        }
        bcmp_ll_element_t *element = _resource_request_list.find(src_node_id);
        if ((element != NULL) && (element->fp != NULL)) {
            element->fp(repl);
        } else {
            printf("Node Id %" PRIx64 " resource table:\n", src_node_id);
            uint16_t num_pubs = repl->num_pubs;
            size_t offset = 0;
            printf("\tPublishers:\n");
            while(num_pubs){
                bcmp_resource_t * cur_resource = reinterpret_cast<bcmp_resource_t *>(&repl->resource_list[offset]);
                printf("\t* %.*s\n",cur_resource->resource_len, cur_resource->resource);
                offset += (sizeof(bcmp_resource_t) + cur_resource->resource_len);
                num_pubs--;
            }
            uint16_t num_subs = repl->num_subs;
            printf("\tSubscribers:\n");
            while(num_subs){
                bcmp_resource_t * cur_resource = reinterpret_cast<bcmp_resource_t *>(&repl->resource_list[offset]);
                printf("\t* %.*s\n",cur_resource->resource_len, cur_resource->resource);
                offset += (sizeof(bcmp_resource_t) + cur_resource->resource_len);
                num_subs--;
            }
        }
        if (element != NULL) {
            _resource_request_list.remove(element);
        }
    } while(0);
}

/*!
  Init the bcmp resource discovery module.

  \return - None
*/
void bcmp_resource_discovery::bcmp_resource_discovery_init(void) {
    _pub_list.start = NULL;
    _pub_list.end = NULL;
    _pub_list.num_resources = 0;
    _pub_list.lock = xSemaphoreCreateMutex();
    configASSERT(_pub_list.lock);
    _sub_list.start = NULL;
    _sub_list.end = NULL;
    _sub_list.num_resources = 0;
    _sub_list.lock = xSemaphoreCreateMutex();
    configASSERT(_sub_list.lock);
}

/*!
  Add a resource to the resource discovery module. Note that you can add this for a topic you intend to publish data
  to ahead of actually publishing the data.

  \param in *res - resource name
  \param in resource_len - length of the resource name
  \param in type - publishers or subscribers
  \param in timeoutMs - how long to wait to add resource in milliseconds.
  \return - true on success, false otherwise
*/
bool bcmp_resource_discovery::bcmp_resource_discovery_add_resource(const char * res, const uint16_t resource_len, resource_type_e type, uint32_t timeoutMs) {
    bool rval = false;
    bcmp_resource_list_t *res_list = (type == SUB) ? &_sub_list : &_pub_list;
    if(xSemaphoreTake(res_list->lock, pdMS_TO_TICKS(timeoutMs)) == pdPASS){
        do {
            // Check for resource
            if(_bcmp_resource_discovery_find_resource(res, resource_len, type)) {
                // Already in list.
                break;
            }
            // Build resouce
            size_t resource_size = sizeof(bcmp_resource_t) + resource_len;
            uint8_t *resource_buffer = reinterpret_cast<uint8_t *>(pvPortMalloc(resource_size));
            configASSERT(resource_buffer);
            bcmp_resource_t * resource = reinterpret_cast<bcmp_resource_t*>(resource_buffer);
            resource->resource_len = resource_len;
            memcpy(resource->resource, res, resource_len);

            // Build Node
            bcmp_resource_node_t *resource_node = reinterpret_cast<bcmp_resource_node_t *>(pvPortMalloc(sizeof(bcmp_resource_node_t)));
            configASSERT(resource_node);
            resource_node->resource = resource;
            resource_node->next = NULL;
            // Add node to list
            if(res_list->start == NULL) { // First resource
                res_list->start = resource_node;
            } else { // 2nd+ resource
                res_list->end->next = resource_node;
            }
            res_list->end = resource_node;
            res_list->num_resources++;
            rval = true;
        } while(0);
        xSemaphoreGive(res_list->lock);
    }
    return rval;
}

/*!
  Get number of resources in the table.

  \param out &num_resources - number of resources in the table.
  \param in type - publishers or subscribers
  \param in timeoutMs - how long to wait to add resource in milliseconds.
  \return - true on success, false otherwise
*/
bool bcmp_resource_discovery::bcmp_resource_discovery_get_num_resources(uint16_t& num_resources, resource_type_e type, uint32_t timeoutMs) {
    bool rval = false;
    bcmp_resource_list_t *res_list = (type == SUB) ? &_sub_list : &_pub_list;
    if(xSemaphoreTake(res_list->lock, pdMS_TO_TICKS(timeoutMs)) == pdPASS){
        num_resources = res_list->num_resources;
        rval = true;
        xSemaphoreGive(res_list->lock);
    }
    return rval;
}


/*!
  Check if a given resource is in the table.

  \param in *res - resource name
  \param in resource_len - length of the resource name
  \param out &found - whether or not the resource was found in the table
  \param in type - publishers or subscribers
  \param in timeoutMs - how long to wait to add resource in milliseconds.
  \return - true on success, false otherwise
*/
bool bcmp_resource_discovery::bcmp_resource_discovery_find_resource(const char * res, const uint16_t resource_len, bool &found, resource_type_e type, uint32_t timeoutMs) {
    bool rval = false;
    bcmp_resource_list_t *res_list = (type == SUB) ? &_sub_list : &_pub_list;
    if(xSemaphoreTake(res_list->lock, pdMS_TO_TICKS(timeoutMs)) == pdPASS){
        found = _bcmp_resource_discovery_find_resource(res, resource_len, type);
        rval = true;
        xSemaphoreGive(res_list->lock);
    }
    return rval;
}

/*!
  Send a bcmp resource discovery request to a node.

  \param in target_node_id - requested node id
  \return - true on success, false otherwise
*/
bool bcmp_resource_discovery::bcmp_resource_discovery_send_request(uint64_t target_node_id, void (*fp)(void*)) {
    bool rval = false;
    do {
        bcmp_resource_table_request_t req = {
            .target_node_id = target_node_id,
        };
        if(bcmp_tx(&multicast_ll_addr, BCMP_RESOURCE_TABLE_REQUEST, reinterpret_cast<uint8_t *>(&req), sizeof(req)) != ERR_OK){
            printf("Failed to send bcmp resource table request\n");
            break;
        }
        _resource_request_list.add(target_node_id, fp);
        rval = true;
    } while(0);
    return rval;
}

/*!
  Print the resources in the table.

  \return - None
*/
void bcmp_resource_discovery::bcmp_resource_discovery_print_resources(void) {
    printf("Resource table:\n");
    if( _pub_list.num_resources && xSemaphoreTake(_pub_list.lock, pdMS_TO_TICKS(DEFAULT_RESOURCE_ADD_TIMEOUT_MS)) == pdPASS) {
        printf("\tPubs:\n");
        uint16_t num_items = _pub_list.num_resources;
        bcmp_resource_node_t * cur_resource_node = _pub_list.start;
        while(num_items && cur_resource_node){
            printf("\t* %.*s\n",cur_resource_node->resource->resource_len, cur_resource_node->resource->resource);
            cur_resource_node = cur_resource_node->next;
            num_items--;
        }
        xSemaphoreGive(_pub_list.lock);
    }
    if( _sub_list.num_resources && xSemaphoreTake(_sub_list.lock, pdMS_TO_TICKS(DEFAULT_RESOURCE_ADD_TIMEOUT_MS)) == pdPASS) {
        printf("\tSubs:\n");
        uint16_t num_items = _sub_list.num_resources;
        bcmp_resource_node_t * cur_resource_node = _sub_list.start;
        while(num_items && cur_resource_node){
            printf("\t* %.*s\n",cur_resource_node->resource->resource_len, cur_resource_node->resource->resource);
            cur_resource_node = cur_resource_node->next;
            num_items--;
        }
        xSemaphoreGive(_sub_list.lock);
    }
}

// TODO: Make this a table for faster lookup.
static bool _bcmp_resource_discovery_find_resource(const char * resource, const uint16_t resource_len, resource_type_e type) {
    bool rval = false;
    bcmp_resource_list_t *res_list = (type == SUB) ? &_sub_list : &_pub_list;
    do {
        bcmp_resource_node_t * cur = res_list->start;
        while(cur) {
            if(memcmp(resource, cur->resource->resource, resource_len) == 0) {
                return true;
            }
            cur = cur->next;
        }
    } while(0);
    return rval;
}

/*!
  Get the resources in the table.

  \return - pointer to the resource table reply, caller is responsible for freeing the memory.
*/
bcmp_resource_table_reply_t* bcmp_resource_discovery::bcmp_resource_discovery_get_local_resources(void) {
    bool success = false;
    bcmp_resource_table_reply_t *reply_rval = NULL;
    size_t msg_len = sizeof(bcmp_resource_table_reply_t);
    do {
        if(!_bcmp_resource_compute_list_size(PUB, msg_len)) {
            printf("Failed to get publishers list\n.");
            break;
        }
        if(!_bcmp_resource_compute_list_size(SUB, msg_len)) {
            printf("Failed to get subscribers list\n.");
            break;
        }
        // Create and fill the reply
        reply_rval = static_cast<bcmp_resource_table_reply_t *>(pvPortMalloc(msg_len));
        configASSERT(reply_rval);
        reply_rval->node_id = getNodeId();
        uint32_t data_offset = 0;
        if (!_bcmp_resource_populate_msg_data(PUB, reply_rval, data_offset)) {
            printf("Failed to populate publishers list\n.");
            break;
        }
        if (!_bcmp_resource_populate_msg_data(SUB, reply_rval, data_offset)) {
            printf("Failed to populate publishers list\n.");
            break;
        }
        success = true;
    } while(0);

    if (!success) {
        if (reply_rval != NULL) {
            vPortFree(reply_rval);
            reply_rval = NULL;
        }
    }
    return reply_rval;
}
