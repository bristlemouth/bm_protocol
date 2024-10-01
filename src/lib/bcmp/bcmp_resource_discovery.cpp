#include "bcmp_resource_discovery.h"
#include "bcmp.h"
#include "bcmp_linked_list_generic.h"
#include "bm_util.h"
#include "string.h"
#include <stdlib.h>
extern "C" {
#include "bm_os.h"
#include "packet.h"
}

typedef struct BcmpResourceNode {
  BcmpResource *resource;
  BcmpResourceNode *next;
} BcmpResourceNode;

typedef struct BcmpResourceList {
  BcmpResourceNode *start;
  BcmpResourceNode *end;
  uint16_t num_resources;
  BmSemaphore lock;
} BcmpResourceList;

static BcmpResourceList PUB_LIST;
static BcmpResourceList SUB_LIST;
static BCMP_Linked_List_Generic RESOURCE_REQUEST_LIST;

static bool bcmp_resource_discovery_find_resource(const char *resource,
                                                  const uint16_t resource_len,
                                                  ResourceType type);
static bool bcmp_resource_compute_list_size(ResourceType type, size_t &msg_len);
static bool bcmp_resource_populate_msg_data(ResourceType type, BcmpResourceTableReply *repl,
                                            uint32_t &data_offset);

static bool bcmp_resource_compute_list_size(ResourceType type, size_t &msg_len) {
  bool rval = false;
  BcmpResourceList *res_list = (type == SUB) ? &SUB_LIST : &PUB_LIST;
  if (bm_semaphore_take(res_list->lock, DEFAULT_RESOURCE_ADD_TIMEOUT_MS) == pdPASS) {
    do {
      BcmpResourceNode *cur = res_list->start;
      while (cur != NULL) {
        msg_len += (sizeof(BcmpResource) + cur->resource->resource_len);
        cur = cur->next;
      }
      rval = true;
    } while (0);
    bm_semaphore_give(res_list->lock);
  }
  return rval;
}

static bool bcmp_resource_populate_msg_data(ResourceType type, BcmpResourceTableReply *repl,
                                            uint32_t &data_offset) {
  bool rval = false;
  BcmpResourceList *res_list = (type == SUB) ? &SUB_LIST : &PUB_LIST;
  if (bm_semaphore_take(res_list->lock, DEFAULT_RESOURCE_ADD_TIMEOUT_MS) == pdPASS) {
    do {
      if (type == PUB) {
        repl->num_pubs = res_list->num_resources;
      } else { // SUB
        repl->num_subs = res_list->num_resources;
      }
      BcmpResourceNode *cur = res_list->start;
      while (cur != NULL) {
        size_t res_size = (sizeof(BcmpResource) + cur->resource->resource_len);
        memcpy(&repl->resource_list[data_offset], cur->resource, res_size);
        data_offset += res_size;
        cur = cur->next;
      }
      rval = true;
    } while (0);
    bm_semaphore_give(res_list->lock);
  }
  return rval;
}

/*!
  Process the resource discovery request message.

  \param in *req - request
  \param in *dst - destination IP to reply to.
  \return - None
*/
static BmErr bcmp_process_resource_discovery_request(BcmpProcessData data) {
  BmErr err = BmEBADMSG;
  BcmpResourceTableRequest *req = (BcmpResourceTableRequest *)data.payload;
  if (req->target_node_id == node_id()) {
    size_t msg_len = sizeof(BcmpResourceTableReply);
    if (!bcmp_resource_compute_list_size(PUB, msg_len)) {
      printf("Failed to get publishers list\n.");
      return err;
    }
    if (!bcmp_resource_compute_list_size(SUB, msg_len)) {
      printf("Failed to get subscribers list\n.");
      return err;
    }
    // Create and fill the reply
    uint8_t *reply_buf = (uint8_t *)bm_malloc(msg_len);
    if (reply_buf) {
      do {
        BcmpResourceTableReply *repl = (BcmpResourceTableReply *)reply_buf;
        repl->node_id = node_id();
        uint32_t data_offset = 0;
        if (!bcmp_resource_populate_msg_data(PUB, repl, data_offset)) {
          printf("Failed to get publishers list\n.");
          break;
        }
        if (!bcmp_resource_populate_msg_data(SUB, repl, data_offset)) {
          printf("Failed to get publishers list\n.");
          break;
        }
        if (bcmp_tx(data.dst, BcmpResourceTableReplyMessage, reply_buf, msg_len) != BmOK) {
          printf("Failed to send bcmp resource table reply\n");
        } else {
          err = BmOK;
        }
      } while (0);
      bm_free(reply_buf);
    }
  }

  return err;
}

/*!
  Process the resource discovery reply message.

  \param in *repl - reply
  \param in src_node_id - node ID of the source.
  \return - None
*/
static BmErr bcmp_process_resource_discovery_reply(BcmpProcessData data) {
  BmErr err = BmEBADMSG;
  BcmpResourceTableReply *repl = (BcmpResourceTableReply *)data.payload;
  uint64_t src_node_id = ip_to_nodeid(data.src);
  if (repl->node_id == src_node_id) {
    bcmp_ll_element_t *element = RESOURCE_REQUEST_LIST.find(src_node_id);

    if ((element != NULL) && (element->fp != NULL)) {
      element->fp(repl);
      err = BmOK;
    } else {
      printf("Node Id %016" PRIx64 " resource table:\n", src_node_id);
      uint16_t num_pubs = repl->num_pubs;
      size_t offset = 0;
      printf("\tPublishers:\n");
      while (num_pubs) {
        BcmpResource *cur_resource = (BcmpResource *)&repl->resource_list[offset];
        printf("\t* %.*s\n", cur_resource->resource_len, cur_resource->resource);
        offset += (sizeof(BcmpResource) + cur_resource->resource_len);
        num_pubs--;
      }
      uint16_t num_subs = repl->num_subs;
      printf("\tSubscribers:\n");
      while (num_subs) {
        BcmpResource *cur_resource = (BcmpResource *)&repl->resource_list[offset];
        printf("\t* %.*s\n", cur_resource->resource_len, cur_resource->resource);
        offset += (sizeof(BcmpResource) + cur_resource->resource_len);
        num_subs--;
      }
      err = BmOK;
    }
    if (element != NULL) {
      RESOURCE_REQUEST_LIST.remove(element);
    }
  }

  return err;
}

/*!
  Init the bcmp resource discovery module.

  \return - None
*/
BmErr bcmp_resource_discovery_init(void) {
  BmErr err = BmENOMEM;
  BcmpPacketCfg resource_request = {
      false,
      false,
      bcmp_process_resource_discovery_request,
  };
  BcmpPacketCfg resource_reply = {
      false,
      false,
      bcmp_process_resource_discovery_reply,
  };

  PUB_LIST.start = NULL;
  PUB_LIST.end = NULL;
  PUB_LIST.num_resources = 0;
  PUB_LIST.lock = bm_semaphore_create();
  SUB_LIST.start = NULL;
  SUB_LIST.end = NULL;
  SUB_LIST.num_resources = 0;
  SUB_LIST.lock = bm_semaphore_create();
  if (PUB_LIST.lock && SUB_LIST.lock) {
    err = BmOK;
  }
  bm_err_check(err, packet_add(&resource_request, BcmpResourceTableReplyMessage));
  bm_err_check(err, packet_add(&resource_reply, BcmpResourceTableReplyMessage));
  return err;
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
bool bcmp_resource_discovery_add_resource(const char *res, const uint16_t resource_len,
                                          ResourceType type, uint32_t timeoutMs) {
  bool rval = false;
  BcmpResourceList *res_list = (type == SUB) ? &SUB_LIST : &PUB_LIST;
  if (bm_semaphore_take(res_list->lock, timeoutMs) == pdPASS) {
    do {
      // Check for resource
      if (bcmp_resource_discovery_find_resource(res, resource_len, type)) {
        // Already in list.
        break;
      }
      // Build resouce
      size_t resource_size = sizeof(BcmpResource) + resource_len;
      uint8_t *resource_buffer = (uint8_t *)bm_malloc(resource_size);
      BcmpResource *resource = (BcmpResource *)resource_buffer;
      resource->resource_len = resource_len;
      memcpy(resource->resource, res, resource_len);

      // Build Node
      BcmpResourceNode *resource_node = (BcmpResourceNode *)bm_malloc(sizeof(BcmpResourceNode));
      if (resource_node) {
        resource_node->resource = resource;
        resource_node->next = NULL;
        // Add node to list
        if (res_list->start == NULL) { // First resource
          res_list->start = resource_node;
        } else { // 2nd+ resource
          res_list->end->next = resource_node;
        }
        res_list->end = resource_node;
        res_list->num_resources++;
        rval = true;
      }
    } while (0);
    bm_semaphore_give(res_list->lock);
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
bool bcmp_resource_discovery_get_num_resources(uint16_t &num_resources, ResourceType type,
                                               uint32_t timeoutMs) {
  bool rval = false;
  BcmpResourceList *res_list = (type == SUB) ? &SUB_LIST : &PUB_LIST;
  if (bm_semaphore_take(res_list->lock, timeoutMs) == pdPASS) {
    num_resources = res_list->num_resources;
    rval = true;
    bm_semaphore_give(res_list->lock);
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
bool bcmp_resource_discovery_find_resource(const char *res, const uint16_t resource_len,
                                           bool &found, ResourceType type, uint32_t timeoutMs) {
  bool rval = false;
  BcmpResourceList *res_list = (type == SUB) ? &SUB_LIST : &PUB_LIST;
  if (bm_semaphore_take(res_list->lock, timeoutMs) == pdPASS) {
    found = bcmp_resource_discovery_find_resource(res, resource_len, type);
    rval = true;
    bm_semaphore_give(res_list->lock);
  }
  return rval;
}

/*!
  Send a bcmp resource discovery request to a node.

  \param in target_node_id - requested node id
  \return - true on success, false otherwise
*/
bool bcmp_resource_discovery_send_request(uint64_t target_node_id, void (*fp)(void *)) {
  bool rval = false;
  do {
    BcmpResourceTableRequest req = {
        .target_node_id = target_node_id,
    };
    if (bcmp_tx(&multicast_ll_addr, BcmpResourceTableRequestMessage, (uint8_t *)&req,
                sizeof(req)) != BmOK) {
      printf("Failed to send bcmp resource table request\n");
      break;
    }
    RESOURCE_REQUEST_LIST.add(target_node_id, fp);
    rval = true;
  } while (0);
  return rval;
}

/*!
  Print the resources in the table.

  \return - None
*/
void bcmp_resource_discovery_print_resources(void) {
  printf("Resource table:\n");
  if (PUB_LIST.num_resources &&
      bm_semaphore_take(PUB_LIST.lock, DEFAULT_RESOURCE_ADD_TIMEOUT_MS) == pdPASS) {
    printf("\tPubs:\n");
    uint16_t num_items = PUB_LIST.num_resources;
    BcmpResourceNode *cur_resource_node = PUB_LIST.start;
    while (num_items && cur_resource_node) {
      printf("\t* %.*s\n", cur_resource_node->resource->resource_len,
             cur_resource_node->resource->resource);
      cur_resource_node = cur_resource_node->next;
      num_items--;
    }
    bm_semaphore_give(PUB_LIST.lock);
  }
  if (SUB_LIST.num_resources &&
      bm_semaphore_take(SUB_LIST.lock, DEFAULT_RESOURCE_ADD_TIMEOUT_MS) == pdPASS) {
    printf("\tSubs:\n");
    uint16_t num_items = SUB_LIST.num_resources;
    BcmpResourceNode *cur_resource_node = SUB_LIST.start;
    while (num_items && cur_resource_node) {
      printf("\t* %.*s\n", cur_resource_node->resource->resource_len,
             cur_resource_node->resource->resource);
      cur_resource_node = cur_resource_node->next;
      num_items--;
    }
    bm_semaphore_give(SUB_LIST.lock);
  }
}

// TODO: Make this a table for faster lookup.
static bool bcmp_resource_discovery_find_resource(const char *resource,
                                                  const uint16_t resource_len,
                                                  ResourceType type) {
  bool rval = false;
  BcmpResourceList *res_list = (type == SUB) ? &SUB_LIST : &PUB_LIST;
  do {
    BcmpResourceNode *cur = res_list->start;
    while (cur) {
      if (memcmp(resource, cur->resource->resource, resource_len) == 0) {
        return true;
      }
      cur = cur->next;
    }
  } while (0);
  return rval;
}

/*!
  Get the resources in the table.

  \return - pointer to the resource table reply, caller is responsible for freeing the memory.
*/
BcmpResourceTableReply *bcmp_resource_discovery_get_local_resources(void) {
  bool success = false;
  BcmpResourceTableReply *reply_rval = NULL;
  size_t msg_len = sizeof(BcmpResourceTableReply);
  do {
    if (!bcmp_resource_compute_list_size(PUB, msg_len)) {
      printf("Failed to get publishers list\n.");
      break;
    }
    if (!bcmp_resource_compute_list_size(SUB, msg_len)) {
      printf("Failed to get subscribers list\n.");
      break;
    }
    // Create and fill the reply
    reply_rval = (BcmpResourceTableReply *)bm_malloc(msg_len);
    if (reply_rval) {
      reply_rval->node_id = node_id();
      uint32_t data_offset = 0;
      if (!bcmp_resource_populate_msg_data(PUB, reply_rval, data_offset)) {
        printf("Failed to populate publishers list\n.");
        break;
      }
      if (!bcmp_resource_populate_msg_data(SUB, reply_rval, data_offset)) {
        printf("Failed to populate publishers list\n.");
        break;
      }
      success = true;
    }
  } while (0);

  if (!success) {
    if (reply_rval != NULL) {
      bm_free(reply_rval);
      reply_rval = NULL;
    }
  }
  return reply_rval;
}
