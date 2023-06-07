#include "bcmp_resource_discovery.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "string.h"
#include <stdlib.h>
#include "device_info.h"
#include "bcmp.h"

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

static bcmp_resource_list_t _list;

static bool _bcmp_resource_discovery_find_resource(const char * resource, const uint16_t resource_len);

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
        if(xSemaphoreTake(_list.lock, pdMS_TO_TICKS(DEFAULT_RESOURCE_ADD_TIMEOUT_MS)) == pdPASS) {
            // Compute the size of the reply
            bcmp_resource_node_t * cur = _list.start;
            while(cur != NULL) {
                msg_len += (sizeof(bcmp_resource_t) + cur->resource->resource_len);
                cur = cur->next;
            }
            // Create and fill the reply
            uint8_t * reply_buf = reinterpret_cast<uint8_t*>(pvPortMalloc(msg_len));
            configASSERT(reply_buf);
            bcmp_resource_table_reply_t * repl = reinterpret_cast<bcmp_resource_table_reply_t *>(reply_buf);
            repl->node_id = getNodeId();
            repl->resource_len = _list.num_resources;
            cur = _list.start;
            uint16_t offset = 0;
            while(cur != NULL) {
                size_t res_size = (sizeof(bcmp_resource_t) + cur->resource->resource_len);
                memcpy(&repl->resource_list[offset], cur->resource, res_size);
                offset += res_size;
                cur = cur->next;
            }
            if(bcmp_tx(dst, BCMP_RESOURCE_TABLE_REPLY, reply_buf, msg_len) != ERR_OK){
                printf("Failed to send bcmp resource table reply\n");
            }
            vPortFree(reply_buf);
            xSemaphoreGive(_list.lock);
        }
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
        printf("Node Id %" PRIx64 " resource table:\n", src_node_id);
        uint16_t num_items = repl->resource_len;
        size_t offset = 0;
        while(num_items){
            bcmp_resource_t * cur_resource = reinterpret_cast<bcmp_resource_t *>(&repl->resource_list[offset]);
            printf("\t* %.*s\n",cur_resource->resource_len, cur_resource->resource);
            offset += (sizeof(bcmp_resource_t) + cur_resource->resource_len);
            num_items--;
        }
    } while(0);
}

/*!
  Init the bcmp resource discovery module.

  \return - None
*/
void bcmp_resource_discovery::bcmp_resource_discovery_init(void) {
    _list.start = NULL;
    _list.end = NULL;
    _list.num_resources = 0;
    _list.lock = xSemaphoreCreateMutex();
    configASSERT(_list.lock);
}

/*!
  Add a resource to the resource discovery module. Note that you can add this for a topic you intend to publish data
  to ahead of actually publishing the data.

  \param in *res - resource name 
  \param in resource_len - length of the resource name
  \param in timeoutMs - how long to wait to add resource in milliseconds.
  \return - true on success, false otherwise
*/
bool bcmp_resource_discovery::bcmp_resource_discovery_add_resource(const char * res, const uint16_t resource_len, uint32_t timeoutMs) {
    bool rval = false;
    if(xSemaphoreTake(_list.lock, pdMS_TO_TICKS(timeoutMs)) == pdPASS){
        do {
            // Check for resource
            if(_bcmp_resource_discovery_find_resource(res, resource_len)) {
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
            if(_list.start == NULL) { // First resource 
                _list.start = resource_node;
            } else { // 2nd+ resource
                _list.end->next = resource_node;
            }
            _list.end = resource_node;
            _list.num_resources++;
            rval = true;
        } while(0);
        xSemaphoreGive(_list.lock);
    }
    return rval;
}

/*!
  Get number of resources in the table.

  \param out &num_resources - number of resources in the table.
  \param in timeoutMs - how long to wait to add resource in milliseconds.
  \return - true on success, false otherwise
*/
bool bcmp_resource_discovery::bcmp_resource_discovery_get_num_resources(uint16_t& num_resources, uint32_t timeoutMs) {
    bool rval = false;
    if(xSemaphoreTake(_list.lock, pdMS_TO_TICKS(timeoutMs)) == pdPASS){
        num_resources = _list.num_resources;
        rval = true;
        xSemaphoreGive(_list.lock);
    }
    return rval;
}


/*!
  Check if a given resource is in the table.

  \param in *res - resource name 
  \param in resource_len - length of the resource name
  \param out &found - whether or not the resource was found in the table
  \param in timeoutMs - how long to wait to add resource in milliseconds.
  \return - true on success, false otherwise
*/
bool bcmp_resource_discovery::bcmp_resource_discovery_find_resource(const char * res, const uint16_t resource_len, bool &found, uint32_t timeoutMs) {
    bool rval = false;
    if(xSemaphoreTake(_list.lock, pdMS_TO_TICKS(timeoutMs)) == pdPASS){
        found = _bcmp_resource_discovery_find_resource(res, resource_len);
        rval = true;
        xSemaphoreGive(_list.lock);
    }
    return rval;
}

/*!
  Send a bcmp resource discovery request to a node.

  \param in target_node_id - requested node id
  \return - true on success, false otherwise
*/
bool bcmp_resource_discovery::bcmp_resource_discovery_send_request(uint64_t target_node_id) {
    bool rval = false;
    do {
        bcmp_resource_table_request_t req = {
            .target_node_id = target_node_id,
        };
        if(bcmp_tx(&multicast_ll_addr, BCMP_RESOURCE_TABLE_REQUEST, reinterpret_cast<uint8_t *>(&req), sizeof(req)) != ERR_OK){
            printf("Failed to send bcmp resource table reply\n");
            break;
        }
        rval = true;
    } while(0);
    return rval;
}

/*!
  Print the resources in the table.

  \return - None
*/
void bcmp_resource_discovery::bcmp_resource_discovery_print_resources(void) {
    if(xSemaphoreTake(_list.lock, pdMS_TO_TICKS(DEFAULT_RESOURCE_ADD_TIMEOUT_MS)) == pdPASS) {
        printf("Resource table:\n");
        uint16_t num_items = _list.num_resources;
        bcmp_resource_node_t * cur_resource_node = _list.start;
        while(num_items && cur_resource_node){
            printf("\t* %.*s\n",cur_resource_node->resource->resource_len, cur_resource_node->resource->resource);
            cur_resource_node = cur_resource_node->next;
            num_items--;
        }
        xSemaphoreGive(_list.lock);
    }
}

// TODO: Make this a table for faster lookup.
static bool _bcmp_resource_discovery_find_resource(const char * resource, const uint16_t resource_len) {
    bool rval = false;
    do {
        bcmp_resource_node_t * cur = _list.start;
        while(cur) {
            if(memcmp(resource, cur->resource->resource, resource_len) == 0) {
                return true;
            }
            cur = cur->next;
        }
    } while(0);
    return rval;
}