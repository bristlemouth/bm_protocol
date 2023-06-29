#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"
#include "task_priorities.h"

#include "bm_l2.h"
#include "bcmp.h"
#include "bcmp_info.h"
#include "bcmp_messages.h"
#include "bcmp_neighbors.h"
#include "bcmp_topology.h"
#include "bm_util.h"
#include "device_info.h"
#include "util.h"

#define BCMP_TOPO_EVT_QUEUE_LEN 32
// Timer to stop waiting for a nodes neighbor table
#define BCMP_TOPO_TIMEOUT_S 1
#define BCMP_TABLE_MAX_LEN 1024

typedef enum {
  BCMP_TOPO_EVT_START,
  BCMP_TOPO_EVT_CHECK_NODE,
  BCMP_TOPO_EVT_ADD_NODE,
  BCMP_TOPO_EVT_TIMEOUT,
  BCMP_TOPO_EVT_RESTART,
  BCMP_TOPO_EVT_END,
} bcmp_topo_queue_type_e;

typedef struct {
  bcmp_topo_queue_type_e type;
  neighborTableEntry_t* neighborEntry;
  bcmp_topo_cb_t callback;
} bcmp_topo_queue_item_t;

typedef struct {
  QueueHandle_t evt_queue;
  TimerHandle_t topo_timer;
  networkTopology_t* networkTopology;
  bool in_progress;
  bcmp_topo_cb_t callback;
} bcmpTopoContext_t;

static bcmpTopoContext_t _ctx;
static TaskHandle_t bcmpTopologyTask = NULL;

static uint64_t _target_node_id = 0;
static bool _sent_request = false;
static bool _insert_before = false;

static void bcmp_topology_thread(void *parameters);

static void freeNeighborTableEntry(neighborTableEntry_t** entry);
static networkTopology_t* newNetworkTopology(void);
static void freeNetworkTopology(networkTopology_t** networkTopology);
static void networkTopologyClear(networkTopology_t* networkTopology);
static void networkTopologyMoveFront(networkTopology_t* networkTopology);
static void networkTopologyMovePrev(networkTopology_t* networkTopology);
static void networkTopologyMoveNext(networkTopology_t* networkTopology);
static void networkTopologyPrepend(networkTopology_t* networkTopology, neighborTableEntry_t *entry);
static void networkTopologyAppend(networkTopology_t* networkTopology, neighborTableEntry_t *entry);
static void networkTopologyInsertAfter(networkTopology_t* networkTopology, neighborTableEntry_t *entry);
static void networkTopologyInsertBefore(networkTopology_t* networkTopology, neighborTableEntry_t *entry);
static void networkTopologyDeleteBack(networkTopology_t* networkTopology);
static void networkTopologyIncrementPortCount(networkTopology_t* networkTopology);
static uint8_t networkTopologyGetPortCount(networkTopology_t* networkTopology);
static bool networkTopologyIsRoot(networkTopology_t* networkTopology);
static bool networkTopologyNodeIdInTopology(networkTopology_t* networkTopology, uint64_t node_id);
static uint64_t networkTopologyCheckNeighborNodeIds(networkTopology_t *networkTopology);
static bool networkTopologyCheckAllPortsExplored(networkTopology_t *networkTopology);

// assembles the neighbor info list
static void _assemble_neighbor_info_list(bcmp_neighbor_info_t *_neighbor_info_list, bm_neighbor_t *neighbor, uint8_t num_neighbors) {
  uint16_t _neighbor_count = 0;
  while(neighbor != NULL && _neighbor_count < num_neighbors) {
    _neighbor_info_list[_neighbor_count].node_id = neighbor->node_id;
    _neighbor_info_list[_neighbor_count].port = neighbor->port;
    _neighbor_info_list[_neighbor_count].online = (uint8_t)neighbor->online;
    _neighbor_count++;
    // Go to the next one
    neighbor = neighbor->next;
  }
}


static void process_start_topology_event(void) {
  // here we will need to kick off the topo process by looking at our own neighbors and then sending out a request
  _ctx.networkTopology = newNetworkTopology();

  static uint8_t num_ports = bm_l2_get_num_ports();

  // Check our neighbors
  uint8_t num_neighbors = 0;
  bm_neighbor_t *neighbor = bcmp_get_neighbors(num_neighbors);

  // here we will assemble the entry for the SM
  uint8_t *neighbor_entry_buff = static_cast<uint8_t *>(pvPortMalloc(sizeof(neighborTableEntry_t)));
  configASSERT(neighbor_entry_buff);

  memset(neighbor_entry_buff, 0, sizeof(neighborTableEntry_t));

  neighborTableEntry_t *neighbor_entry = reinterpret_cast<neighborTableEntry_t *>(neighbor_entry_buff);

  uint32_t neighbor_table_len = sizeof(bcmp_neighbor_table_reply_t) + sizeof(bcmp_port_info_t) * num_ports + sizeof(bcmp_neighbor_info_t) * num_neighbors;
  neighbor_entry->neighbor_table_reply = static_cast<bcmp_neighbor_table_reply_t *>(pvPortMalloc(neighbor_table_len));

  neighbor_entry->neighbor_table_reply->node_id = getNodeId();

  // set the other vars
  neighbor_entry->neighbor_table_reply->port_len = num_ports;
  neighbor_entry->neighbor_table_reply->neighbor_len = num_neighbors;

  // assemble the port list here
  for(uint8_t port = 0; port < num_ports; port++) {
    neighbor_entry->neighbor_table_reply->port_list[port].state = bm_l2_get_port_state(port);
  }

  _assemble_neighbor_info_list(reinterpret_cast<bcmp_neighbor_info_t *>(&neighbor_entry->neighbor_table_reply->port_list[num_ports]), neighbor, num_neighbors);

  // this is the root
  neighbor_entry->is_root = true;

  networkTopologyAppend(_ctx.networkTopology, neighbor_entry);
  networkTopologyMoveFront(_ctx.networkTopology);

  bcmp_topo_queue_item_t check_item = {BCMP_TOPO_EVT_CHECK_NODE, NULL, NULL};
  configASSERT(xQueueSend(_ctx.evt_queue, &check_item, 0) == pdTRUE);
}


static void process_check_node_event(void) {
  if (networkTopologyCheckAllPortsExplored(_ctx.networkTopology)) {
    if (networkTopologyIsRoot(_ctx.networkTopology)) {
      bcmp_topo_queue_item_t end_item = {BCMP_TOPO_EVT_END, NULL, NULL};
      configASSERT(xQueueSend(_ctx.evt_queue, &end_item, 0) == pdTRUE);
    } else {
      if(_insert_before) {
        networkTopologyMoveNext(_ctx.networkTopology);
      } else {
        networkTopologyMovePrev(_ctx.networkTopology);
      }
      bcmp_topo_queue_item_t check_item = {BCMP_TOPO_EVT_CHECK_NODE, NULL, NULL};
      configASSERT(xQueueSend(_ctx.evt_queue, &check_item, 0) == pdTRUE);
    }
  } else {
    if (networkTopologyIsRoot(_ctx.networkTopology) && networkTopologyGetPortCount(_ctx.networkTopology) == 1) {
      _insert_before = true;
    }
    uint64_t new_node = networkTopologyCheckNeighborNodeIds(_ctx.networkTopology);
    networkTopologyIncrementPortCount(_ctx.networkTopology);
    if (new_node) {
      bcmp_request_neighbor_table(new_node, &multicast_global_addr);
    } else {
      if (_insert_before) {
        networkTopologyMoveNext(_ctx.networkTopology);
      } else {
        networkTopologyMovePrev(_ctx.networkTopology);
      }
      bcmp_topo_queue_item_t check_item = {BCMP_TOPO_EVT_CHECK_NODE, NULL, NULL};
      configASSERT(xQueueSend(_ctx.evt_queue, &check_item, 0) == pdTRUE);
    }
  }
}

/*!
  Send neighbor table request to node(s)

  \param target_node_id - target node id to send request to (0 for all nodes)
  \param *addr - ip address to send to send request to
  \ret ERR_OK if successful
*/
err_t bcmp_request_neighbor_table(uint64_t target_node_id, const ip_addr_t *addr) {
  bcmp_neighbor_table_request_t neighbor_table_req = {
    .target_node_id=target_node_id
  };
  _target_node_id = target_node_id;
  configASSERT(xTimerStart(_ctx.topo_timer, 10));
  return bcmp_tx(addr, BCMP_NEIGHBOR_TABLE_REQUEST, (uint8_t *)&neighbor_table_req, sizeof(neighbor_table_req));
}

/*!
  Send reply to neighbor table request

  \param *neighbor_table_reply - reply message
  \param *addr - ip address to send reply to
  \ret ERR_OK if successful
*/
err_t bcmp_send_neighbor_table(const ip_addr_t *addr) {
  (void) addr;

  static uint8_t num_ports = bm_l2_get_num_ports();

  // Check our neighbors
  uint8_t num_neighbors = 0;
  bm_neighbor_t *neighbor = bcmp_get_neighbors(num_neighbors);

  uint16_t neighbor_table_len = sizeof(bcmp_neighbor_table_reply_t) +
                                sizeof(bcmp_port_info_t) * num_ports +
                                sizeof(bcmp_neighbor_info_t) * num_neighbors;

  // TODO - handle more gracefully
  if ( neighbor_table_len > BCMP_TABLE_MAX_LEN) {
    return ERR_BUF;
  }

  uint8_t *neighbor_table_reply_buff = static_cast<uint8_t *>(pvPortMalloc(neighbor_table_len));
  configASSERT(neighbor_table_reply_buff);

  memset(neighbor_table_reply_buff, 0, neighbor_table_len);

  bcmp_neighbor_table_reply_t *neighbor_table_reply = reinterpret_cast<bcmp_neighbor_table_reply_t *>(neighbor_table_reply_buff);
  neighbor_table_reply->node_id = getNodeId();

  // set the other vars
  neighbor_table_reply->port_len = num_ports;
  neighbor_table_reply->neighbor_len = num_neighbors;

  // assemble the port list here
  for(uint8_t port = 0; port < num_ports; port++) {
    neighbor_table_reply->port_list[port].state = bm_l2_get_port_state(port);
  }

  _assemble_neighbor_info_list(reinterpret_cast<bcmp_neighbor_info_t *>(&neighbor_table_reply->port_list[num_ports]), neighbor, num_neighbors);

  err_t rval =  bcmp_tx(addr, BCMP_NEIGHBOR_TABLE_REPLY, reinterpret_cast<uint8_t *>(neighbor_table_reply), neighbor_table_len);

  vPortFree(neighbor_table_reply_buff);

  return rval;
}

/*!
  Handle neighbor table requests

  \param *neighbor_table_req - message to process
  \param *src - source ip of requester
  \param *dst - destination ip of request (used for responding to the correct multicast address)
  \ret ERR_OK if successful
*/
err_t bcmp_process_neighbor_table_request(bcmp_neighbor_table_request_t *neighbor_table_req, const ip_addr_t *src, const ip_addr_t *dst) {
  ( void ) src;
  configASSERT(neighbor_table_req);
  if((neighbor_table_req->target_node_id == 0) || getNodeId() == neighbor_table_req->target_node_id) {
    return bcmp_send_neighbor_table(dst);
  }
  return ERR_OK;
}

/*!
  Handle neighbor table replies

  \param *neighbor_table_reply - reply message to process
  \param *src - source ip of requester
  \param *dst - destination ip of request (used for responding to the correct multicast address)
  \
*/
err_t bcmp_process_neighbor_table_reply(bcmp_neighbor_table_reply_t *neighbor_table_reply) {
  configASSERT(neighbor_table_reply);
  if (_target_node_id == neighbor_table_reply->node_id && _sent_request) {

    configASSERT(xTimerStop(_ctx.topo_timer, 10));
    // here we will assemble the entry for the SM
    // malloc the buffer then memcpy it over
    uint8_t *neighbor_entry_buff = static_cast<uint8_t *>(pvPortMalloc(sizeof(neighborTableEntry_t)));
    configASSERT(neighbor_entry_buff);

    memset(neighbor_entry_buff, 0, sizeof(neighborTableEntry_t));
    neighborTableEntry_t *neighbor_entry = reinterpret_cast<neighborTableEntry_t *>(neighbor_entry_buff);

    uint16_t neighbor_table_len = sizeof(bcmp_neighbor_table_reply_t) +
                                  sizeof(bcmp_port_info_t) * neighbor_table_reply->port_len +
                                  sizeof(bcmp_neighbor_info_t) * neighbor_table_reply->neighbor_len;
    neighbor_entry->neighbor_table_reply = static_cast<bcmp_neighbor_table_reply_t *>(pvPortMalloc(neighbor_table_len));

    memcpy(neighbor_entry->neighbor_table_reply, neighbor_table_reply, neighbor_table_len);

    bcmp_topo_queue_item_t item = {
      .type = BCMP_TOPO_EVT_ADD_NODE,
      .neighborEntry = neighbor_entry,
      .callback = NULL
    };

    configASSERT(xQueueSend(_ctx.evt_queue, &item, 0) == pdTRUE);
  }

  return ERR_OK;
}


/*
  FreeRTOS timer handler for timeouts while waiting for neigbor tables. No work is done in the timer
  handler, but instead an event is queued up to be handled in the BCMP task.

  \param tmr unused
  \return none
*/
static void topology_timer_handler(TimerHandle_t tmr){
  (void) tmr;

  bcmp_topo_queue_item_t item = {BCMP_TOPO_EVT_TIMEOUT, NULL, NULL};

  configASSERT(xQueueSend(_ctx.evt_queue, &item, 0) == pdTRUE);
}

/*
  BCMP Topology Task. Handles stepping through the network to request node's neighbor
  tables. Callback can be assigned/called to do something with the assembled network
  topology.

  \param parameters unused
  \return none
*/
static void bcmp_topology_thread(void *parameters) {
  (void) parameters;

  _ctx.topo_timer = xTimerCreate("topology_timer", pdMS_TO_TICKS(BCMP_TOPO_TIMEOUT_S * 1000),
                                 pdFALSE, NULL, topology_timer_handler);
  configASSERT(_ctx.topo_timer);

  for (;;) {
    bcmp_topo_queue_item_t item;

    BaseType_t rval = xQueueReceive(_ctx.evt_queue, &item, portMAX_DELAY);
    configASSERT(rval == pdTRUE);

    switch(item.type) {
      case BCMP_TOPO_EVT_START: {
        if (!_ctx.in_progress){
          _ctx.in_progress = true;
          _sent_request = true;
          _ctx.callback = item.callback;
          process_start_topology_event();
        } else {
          item.callback(NULL);
        }
        break;
      }

      case BCMP_TOPO_EVT_ADD_NODE: {
        if (item.neighborEntry) {
          if(!networkTopologyNodeIdInTopology(_ctx.networkTopology, item.neighborEntry->neighbor_table_reply->node_id)){
            if (_insert_before) {
              networkTopologyInsertBefore(_ctx.networkTopology, item.neighborEntry);
              networkTopologyMovePrev(_ctx.networkTopology);
            } else {
              networkTopologyInsertAfter(_ctx.networkTopology, item.neighborEntry);
              networkTopologyMoveNext(_ctx.networkTopology);
            }
            networkTopologyIncrementPortCount(_ctx.networkTopology); // we have come from one of the ports so it must have been checked
          }
        }
        bcmp_topo_queue_item_t check_item = {BCMP_TOPO_EVT_CHECK_NODE, NULL, NULL};
        configASSERT(xQueueSend(_ctx.evt_queue, &check_item, 0) == pdTRUE);
        break;
      }

      case BCMP_TOPO_EVT_CHECK_NODE: {
        process_check_node_event();
        break;
      }

      case BCMP_TOPO_EVT_END: {
        if (_ctx.callback) {
          _ctx.callback(_ctx.networkTopology);
        }
        // Free the network topology now that we are done
        freeNetworkTopology(&_ctx.networkTopology);
        _insert_before = false;
        _sent_request = false;
        _ctx.in_progress = false;
        break;
      }

      case BCMP_TOPO_EVT_TIMEOUT: {
        if(_insert_before) {
          networkTopologyMoveNext(_ctx.networkTopology);
        } else {
          networkTopologyMovePrev(_ctx.networkTopology);
        }
        bcmp_topo_queue_item_t check_item = {BCMP_TOPO_EVT_CHECK_NODE, NULL, NULL};
        configASSERT(xQueueSend(_ctx.evt_queue, &check_item, 0) == pdTRUE);
        break;
      }

      default: {
        break;
      }
    }
  }
}

void bcmp_topology_start(bcmp_topo_cb_t callback) {

  // create the task if it is not already created
  if (!bcmpTopologyTask) {
    _ctx.evt_queue = xQueueCreate(BCMP_TOPO_EVT_QUEUE_LEN, sizeof(bcmp_topo_queue_item_t));
    configASSERT(_ctx.evt_queue);

    BaseType_t rval = xTaskCreate(bcmp_topology_thread,
                       "BCMP_TOPO",
                       1024,
                       NULL,
                       BCMP_TOPO_TASK_PRIORITY,
                       &bcmpTopologyTask);
    configASSERT(rval == pdPASS);
  }

  // send the first request out
  bcmp_topo_queue_item_t item = {BCMP_TOPO_EVT_START, NULL, callback};
  configASSERT(xQueueSend(_ctx.evt_queue, &item, 0) == pdTRUE);
}

// free a neighbor table entry that is within the network topology
static void freeNeighborTableEntry(neighborTableEntry_t** entry){

  if(entry != NULL && *entry != NULL) {
    vPortFree((*entry)->neighbor_table_reply);
    vPortFree(*entry);
    *entry = NULL;
  }

}
static networkTopology_t* newNetworkTopology(void){
  networkTopology_t* networkTopology = static_cast<networkTopology_t *>(pvPortMalloc(sizeof(networkTopology_t)));
  memset(networkTopology, 0, sizeof(networkTopology_t));
  networkTopology->index = -1;
  return networkTopology;
}

static void freeNetworkTopology(networkTopology_t** networkTopology){
  if(networkTopology != NULL && *networkTopology != NULL) {
    networkTopologyClear(*networkTopology);
    vPortFree(*networkTopology);
    *networkTopology = NULL;
  }
}

static void networkTopologyClear(networkTopology_t* networkTopology){
  if (networkTopology) {
    while(networkTopology->length) {
      networkTopologyDeleteBack(networkTopology);
    }
  }
}

static void networkTopologyMoveFront(networkTopology_t* networkTopology){
  if (networkTopology && networkTopology->length) {
    networkTopology->cursor = networkTopology->front;
    networkTopology->index = 0;
  }
}

static void networkTopologyMovePrev(networkTopology_t* networkTopology){
  if (networkTopology) {
    if (networkTopology->index > 0) {
      networkTopology->cursor = networkTopology->cursor->prevNode;
      networkTopology->index--;
    }
  }
}

static void networkTopologyMoveNext(networkTopology_t* networkTopology){
  if (networkTopology) {
    if (networkTopology->index < networkTopology->length - 1) {
      networkTopology->cursor = networkTopology->cursor->nextNode;
      networkTopology->index++;
    }
  }
}

// insert at the front
static void networkTopologyPrepend(networkTopology_t* networkTopology, neighborTableEntry_t *entry){
  if (networkTopology && entry) {
    if (networkTopology->length == 0) {
      networkTopology->front = entry;
      networkTopology->back = entry;
      networkTopology->front->prevNode = NULL;
      networkTopology->front->nextNode = NULL;
      networkTopology->index++;
    } else {
      entry->nextNode = networkTopology->front;
      networkTopology->front->prevNode = entry;
      networkTopology->front = entry;
    }
    networkTopology->length++;
    if (networkTopology->index >= 0){
      networkTopology->index++;
    }
  }
}

// insert at the end
static void networkTopologyAppend(networkTopology_t* networkTopology, neighborTableEntry_t *entry){
  if (networkTopology && entry) {
    if (networkTopology->length == 0) {
      networkTopology->front = entry;
      networkTopology->back = entry;
      networkTopology->front->prevNode = NULL;
      networkTopology->front->nextNode = NULL;
      networkTopology->index++;
    } else {
      entry->prevNode = networkTopology->back;
      networkTopology->back->nextNode = entry;
      networkTopology->back = entry;
    }
    networkTopology->length++;
  }
}

// insert after the cursor entry
static void networkTopologyInsertAfter(networkTopology_t* networkTopology, neighborTableEntry_t *entry){
  if (networkTopology && entry) {
    if (networkTopology->length == 0) {
      networkTopologyAppend(networkTopology, entry);
    } else if (networkTopology->index == networkTopology->length - 1) {
      networkTopologyAppend(networkTopology, entry);
    } else {
      entry->nextNode = networkTopology->cursor->nextNode;
      networkTopology->cursor->nextNode->prevNode = entry;
      entry->prevNode = networkTopology->cursor;
      networkTopology->cursor->nextNode = entry;
      networkTopology->length++;
    }
  }
}

// insert before the cursor entry
static void networkTopologyInsertBefore(networkTopology_t* networkTopology, neighborTableEntry_t *entry){
  if (networkTopology && entry) {
    if (networkTopology->length == 0) {
      networkTopologyPrepend(networkTopology, entry);
    } else if (networkTopology->index == 0) {
      networkTopologyPrepend(networkTopology, entry);
    } else {
      networkTopology->cursor->prevNode->nextNode = entry;
      entry->nextNode = networkTopology->cursor;
      entry->prevNode = networkTopology->cursor->prevNode;
      networkTopology->cursor->prevNode = entry;
      networkTopology->length++;
      networkTopology->index++;
    }
  }
}

// delete the last entry
static void networkTopologyDeleteBack(networkTopology_t* networkTopology){
    neighborTableEntry_t *temp_entry = NULL;

  if (networkTopology && networkTopology->length) {
    temp_entry = networkTopology->back;
    if (networkTopology->length > 1) {
      networkTopology->back = networkTopology->back->prevNode;
      networkTopology->back->nextNode = NULL;
      if (networkTopology->index == networkTopology->length-1) {
        networkTopology->cursor = networkTopology->back;
      }
    } else {
      networkTopology->cursor = NULL;
      networkTopology->back = NULL;
      networkTopology->front = NULL;
      networkTopology->index = -1;
    }
    networkTopology->length--;
    freeNeighborTableEntry(&temp_entry);
  }
}

static void networkTopologyIncrementPortCount(networkTopology_t* networkTopology) {
  if(networkTopology && networkTopology->cursor) {
    networkTopology->cursor->ports_explored++;
  }
}

static uint8_t networkTopologyGetPortCount(networkTopology_t* networkTopology) {
  if(networkTopology && networkTopology->cursor) {
    return networkTopology->cursor->ports_explored;
  }
  return -1;
}

static bool networkTopologyIsRoot(networkTopology_t* networkTopology){
  if(networkTopology && networkTopology->cursor) {
    return networkTopology->cursor->is_root;
  }
  return false;
}

static bool networkTopologyNodeIdInTopology(networkTopology_t *networkTopology, uint64_t node_id) {
  neighborTableEntry_t *temp_entry;
  if (networkTopology) {
    for(temp_entry = networkTopology->front; temp_entry != NULL; temp_entry = temp_entry->nextNode) {
      if (temp_entry->neighbor_table_reply->node_id == node_id){
        return true;
      }
    }
  }
  return false;
}

static uint64_t networkTopologyCheckNeighborNodeIds(networkTopology_t *networkTopology) {
  if (networkTopology) {
    for (uint8_t neighbors_count = 0; neighbors_count < networkTopology->cursor->neighbor_table_reply->neighbor_len; neighbors_count++) {
      bcmp_neighbor_table_reply_t *temp_table = networkTopology->cursor->neighbor_table_reply;
      bcmp_neighbor_info_t *neighbor_info = reinterpret_cast<bcmp_neighbor_info_t *>(&temp_table->port_list[temp_table->port_len]);
      uint64_t node_id = neighbor_info[neighbors_count].node_id;
      if (!networkTopologyNodeIdInTopology(networkTopology, node_id)) {
        return node_id;
      }
    }
  }
  return 0;
}

static bool networkTopologyCheckAllPortsExplored(networkTopology_t *networkTopology) {
  if (networkTopology) {
    uint16_t neighbors_online_count = 0;
    neighborTableEntry_t *cursor_entry = networkTopology->cursor;
    uint8_t num_ports = cursor_entry->neighbor_table_reply->port_len;
    bcmp_neighbor_info_t *neighbor_info = reinterpret_cast<bcmp_neighbor_info_t *>(&cursor_entry->neighbor_table_reply->port_list[num_ports]);

    // loop through the neighbors and make sure they port is up
    for(uint16_t neighbor_count = 0; neighbor_count < networkTopology->cursor->neighbor_table_reply->neighbor_len; neighbor_count++) {
      // get the port number that is associated with a neighbor
      uint8_t neighbors_port_number = neighbor_info[neighbor_count].port - 1; // subtract 1 since we save the ports as 1,2,... (this means a port cannot be labeled '0')
      if (networkTopology->cursor->neighbor_table_reply->port_list[neighbors_port_number].state) {
        neighbors_online_count++;
      }
    }

    if (neighbors_online_count == networkTopology->cursor->ports_explored) {
      return true;
    }
  }
  return false;
}

void networkTopologyPrint(networkTopology_t* networkTopology){
  if (networkTopology) {
    neighborTableEntry_t *temp_entry;
    for(temp_entry = networkTopology->front; temp_entry != NULL; temp_entry = temp_entry->nextNode) {
      if (temp_entry->prevNode){
        bcmp_neighbor_table_reply_t *temp_table = temp_entry->neighbor_table_reply;
        bcmp_neighbor_info_t *neighbor_info = reinterpret_cast<bcmp_neighbor_info_t *>(&temp_table->port_list[temp_table->port_len]);
        for (uint16_t neighbor_count = 0; neighbor_count < temp_table->neighbor_len; neighbor_count++) {
          if (temp_entry->prevNode->neighbor_table_reply->node_id == neighbor_info[neighbor_count].node_id) {
            printf("%u:", neighbor_info[neighbor_count].port);
          }
        }
      }
      if (temp_entry->is_root) {
        printf("(root)%" PRIx64 "", temp_entry->neighbor_table_reply->node_id);
      } else {
        printf("%" PRIx64 "", temp_entry->neighbor_table_reply->node_id);
      }
      if (temp_entry->nextNode){
        bcmp_neighbor_table_reply_t *temp_table = temp_entry->neighbor_table_reply;
        bcmp_neighbor_info_t *neighbor_info = reinterpret_cast<bcmp_neighbor_info_t *>(&temp_table->port_list[temp_table->port_len]);
        for (uint16_t neighbor_count = 0; neighbor_count < temp_table->neighbor_len; neighbor_count++) {
          if (temp_entry->nextNode->neighbor_table_reply->node_id == neighbor_info[neighbor_count].node_id) {
            printf(":%u", neighbor_info[neighbor_count].port);
          }
        }
      }
      if(temp_entry != networkTopology->back && temp_entry->nextNode != NULL){
        printf(" | ");
      }
    }
    printf("\n");
  } else {
    printf("NULL topology, thread may be busy servicing another topology request\n");
  }
}
