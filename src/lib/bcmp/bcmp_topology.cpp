#include "task_priorities.h"
#include <string.h>

#include "bcmp.h"
#include "bcmp_messages.h"
#include "bcmp_neighbors.h"
#include "bcmp_topology.h"
#include "bm_l2.h"
#include "bm_util.h"
#include "messages.h"

extern "C" {
#include "bm_os.h"
#include "packet.h"
#include "util.h"
}

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
} BcmpTopoQueueType;

typedef struct {
  BcmpTopoQueueType type;
  neighborTableEntry_t *neighbor_entry;
  BcmpTopoCb callback;
} BcmpQueueItem;

typedef struct {
  BmQueue evt_queue;
  BmTimer topo_timer;
  networkTopology_t *network_topology;
  bool in_progress;
  BcmpTopoCb callback;
} BcmpTopoContext;

static BcmpTopoContext CTX;
static TaskHandle_t BCMP_TOPOLOGY_TASK = NULL;

static uint64_t TARGET_NODE_ID = 0;
static bool SENT_REQUEST = false;
static bool INSERT_BEFORE = false;

static void bcmp_topology_thread(void *parameters);
static void free_neighbor_table_entry(neighborTableEntry_t **entry);
static networkTopology_t *new_network_topology(void);
static void free_network_topology(networkTopology_t **network_topology);
static void network_topology_clear(networkTopology_t *network_topology);
static void network_topology_move_to_front(networkTopology_t *network_topology);
static void network_topology_move_prev(networkTopology_t *network_topology);
static void network_topology_move_next(networkTopology_t *network_topology);
static void network_topology_prepend(networkTopology_t *network_topology,
                                     neighborTableEntry_t *entry);
static void network_topology_append(networkTopology_t *network_topology,
                                    neighborTableEntry_t *entry);
static void network_topology_insert_after(networkTopology_t *network_topology,
                                          neighborTableEntry_t *entry);
static void network_topology_insert_before(networkTopology_t *network_topology,
                                           neighborTableEntry_t *entry);
static void network_topology_delete_back(networkTopology_t *network_topology);
static void network_topology_increment_port_count(networkTopology_t *network_topology);
static uint8_t network_topology_get_port_count(networkTopology_t *network_topology);
static bool network_topology_is_root(networkTopology_t *network_topology);
static bool network_topology_node_id_in_topology(networkTopology_t *network_topology,
                                                 uint64_t node_id);
static uint64_t network_topology_check_neighbor_node_ids(networkTopology_t *network_topology);
static bool network_topology_check_all_ports_explored(networkTopology_t *network_topology);

// assembles the neighbor info list
static void assemble_neighbor_info_list(BcmpNeighborInfo *neighbor_info_list,
                                        BcmpNeighbor *neighbor, uint8_t num_neighbors) {
  uint16_t neighbor_count = 0;
  while (neighbor != NULL && neighbor_count < num_neighbors) {
    neighbor_info_list[neighbor_count].node_id = neighbor->node_id;
    neighbor_info_list[neighbor_count].port = neighbor->port;
    neighbor_info_list[neighbor_count].online = (uint8_t)neighbor->online;
    neighbor_count++;
    // Go to the next one
    neighbor = neighbor->next;
  }
}

static void process_start_topology_event(void) {
  // here we will need to kick off the topo process by looking at our own neighbors and then sending out a request
  CTX.network_topology = new_network_topology();

  static uint8_t num_ports = bm_l2_get_num_ports();

  // Check our neighbors
  uint8_t num_neighbors = 0;
  BcmpNeighbor *neighbor = bcmp_get_neighbors(num_neighbors);

  // here we will assemble the entry for the SM
  uint8_t *neighbor_entry_buff = (uint8_t *)bm_malloc(sizeof(neighborTableEntry_t));

  if (neighbor_entry_buff) {
    memset(neighbor_entry_buff, 0, sizeof(neighborTableEntry_t));

    neighborTableEntry_t *neighbor_entry = (neighborTableEntry_t *)(neighbor_entry_buff);

    uint32_t neighbor_table_len = sizeof(BcmpNeighborTableReply) +
                                  sizeof(BcmpPortInfo) * num_ports +
                                  sizeof(BcmpNeighborInfo) * num_neighbors;
    neighbor_entry->neighbor_table_reply =
        (BcmpNeighborTableReply *)bm_malloc(neighbor_table_len);

    neighbor_entry->neighbor_table_reply->node_id = node_id();

    // set the other vars
    neighbor_entry->neighbor_table_reply->port_len = num_ports;
    neighbor_entry->neighbor_table_reply->neighbor_len = num_neighbors;

    // assemble the port list here
    for (uint8_t port = 0; port < num_ports; port++) {
      neighbor_entry->neighbor_table_reply->port_list[port].state = bm_l2_get_port_state(port);
    }

    assemble_neighbor_info_list(
        (BcmpNeighborInfo *)&neighbor_entry->neighbor_table_reply->port_list[num_ports],
        neighbor, num_neighbors);

    // this is the root
    neighbor_entry->is_root = true;

    network_topology_append(CTX.network_topology, neighbor_entry);
    network_topology_move_to_front(CTX.network_topology);

    BcmpQueueItem check_item = {BCMP_TOPO_EVT_CHECK_NODE, NULL, NULL};
    bm_queue_send(CTX.evt_queue, &check_item, 0);
  }
}

static void process_check_node_event(void) {
  if (network_topology_check_all_ports_explored(CTX.network_topology)) {
    if (network_topology_is_root(CTX.network_topology)) {
      BcmpQueueItem end_item = {BCMP_TOPO_EVT_END, NULL, NULL};
      bm_queue_send(CTX.evt_queue, &end_item, 0);
    } else {
      if (INSERT_BEFORE) {
        network_topology_move_next(CTX.network_topology);
      } else {
        network_topology_move_prev(CTX.network_topology);
      }
      BcmpQueueItem check_item = {BCMP_TOPO_EVT_CHECK_NODE, NULL, NULL};
      bm_queue_send(CTX.evt_queue, &check_item, 0);
    }
  } else {
    if (network_topology_is_root(CTX.network_topology) &&
        network_topology_get_port_count(CTX.network_topology) == 1) {
      INSERT_BEFORE = true;
    }
    uint64_t new_node = network_topology_check_neighbor_node_ids(CTX.network_topology);
    network_topology_increment_port_count(CTX.network_topology);
    if (new_node) {
      bcmp_request_neighbor_table(new_node, &multicast_global_addr);
    } else {
      if (INSERT_BEFORE) {
        network_topology_move_next(CTX.network_topology);
      } else {
        network_topology_move_prev(CTX.network_topology);
      }
      BcmpQueueItem check_item = {BCMP_TOPO_EVT_CHECK_NODE, NULL, NULL};
      bm_queue_send(CTX.evt_queue, &check_item, 0);
    }
  }
}

/*!
  @brief Send neighbor table request to node(s)

  @param target_node_id - target node id to send request to (0 for all nodes)
  @param *addr - ip address to send to send request to
  @ret ERR_OK if successful
*/
BmErr bcmp_request_neighbor_table(uint64_t target_node_id, const void *addr) {
  bcmp_neighbor_table_request_t neighbor_table_req = {.target_node_id = target_node_id};
  TARGET_NODE_ID = target_node_id;
  bm_timer_start(CTX.topo_timer, 10);
  return bcmp_tx(addr, BcmpNeighborTableRequestMessage, (uint8_t *)&neighbor_table_req,
                 sizeof(neighbor_table_req));
}

/*!
  @brief Send reply to neighbor table request

  @param *neighbor_table_reply - reply message
  @param *addr - ip address to send reply to
  @ret ERR_OK if successful
*/
static BmErr bcmp_send_neighbor_table(void *addr) {
  static uint8_t num_ports = bm_l2_get_num_ports();
  BmErr err = BmENOMEM;

  // Check our neighbors
  uint8_t num_neighbors = 0;
  BcmpNeighbor *neighbor = bcmp_get_neighbors(num_neighbors);

  uint16_t neighbor_table_len = sizeof(BcmpNeighborTableReply) +
                                sizeof(BcmpPortInfo) * num_ports +
                                sizeof(BcmpNeighborInfo) * num_neighbors;

  // TODO - handle more gracefully
  if (neighbor_table_len > BCMP_TABLE_MAX_LEN) {
    return BmEINVAL;
  }

  uint8_t *neighbor_table_reply_buff = (uint8_t *)bm_malloc(neighbor_table_len);
  if (neighbor_table_reply_buff) {
    memset(neighbor_table_reply_buff, 0, neighbor_table_len);

    BcmpNeighborTableReply *neighbor_table_reply =
        (BcmpNeighborTableReply *)neighbor_table_reply_buff;
    neighbor_table_reply->node_id = node_id();

    // set the other vars
    neighbor_table_reply->port_len = num_ports;
    neighbor_table_reply->neighbor_len = num_neighbors;

    // assemble the port list here
    for (uint8_t port = 0; port < num_ports; port++) {
      neighbor_table_reply->port_list[port].state = bm_l2_get_port_state(port);
    }

    assemble_neighbor_info_list((BcmpNeighborInfo *)&neighbor_table_reply->port_list[num_ports],
                                neighbor, num_neighbors);

    err = bcmp_tx(addr, BcmpNeighborTableReplyMessage, (uint8_t *)neighbor_table_reply,
                  neighbor_table_len);

    bm_free(neighbor_table_reply_buff);
  }

  return err;
}

/*!
  @brief Handle neighbor table requests

  @param *neighbor_table_req - message to process
  @param *src - source ip of requester
  @param *dst - destination ip of request (used for responding to the correct multicast address)
  @return BmOK if successful
  @return BmErr if failed
*/
static BmErr bcmp_process_neighbor_table_request(BcmpProcessData data) {
  BmErr err = BmEINVAL;
  BcmpNeighborTableRequest *request = (BcmpNeighborTableRequest *)data.payload;
  if (request && ((request->target_node_id == 0) || node_id() == request->target_node_id)) {
    err = bcmp_send_neighbor_table(data.dst);
  }
  return err;
}

/*!
  @brief Handle neighbor table replies

  @param *neighbor_table_reply - reply message to process
*/
static BmErr bcmp_process_neighbor_table_reply(BcmpProcessData data) {
  BmErr err = BmEINVAL;
  BcmpNeighborTableReply *reply = (BcmpNeighborTableReply *)data.payload;

  if (TARGET_NODE_ID == reply->node_id && SENT_REQUEST) {
    err = bm_timer_stop(CTX.topo_timer, 10);
    // here we will assemble the entry for the SM
    // malloc the buffer then memcpy it over
    uint8_t *neighbor_entry_buff = (uint8_t *)bm_malloc(sizeof(neighborTableEntry_t));
    if (err == BmOK && neighbor_entry_buff) {

      memset(neighbor_entry_buff, 0, sizeof(neighborTableEntry_t));
      neighborTableEntry_t *neighbor_entry = (neighborTableEntry_t *)neighbor_entry_buff;

      uint16_t neighbor_table_len = sizeof(BcmpNeighborTableReply) +
                                    sizeof(BcmpPortInfo) * reply->port_len +
                                    sizeof(BcmpNeighborInfo) * reply->neighbor_len;
      neighbor_entry->neighbor_table_reply =
          (BcmpNeighborTableReply *)bm_malloc(neighbor_table_len);

      memcpy(neighbor_entry->neighbor_table_reply, reply, neighbor_table_len);

      BcmpQueueItem item = {
          .type = BCMP_TOPO_EVT_ADD_NODE, .neighbor_entry = neighbor_entry, .callback = NULL};

      bm_queue_send(CTX.evt_queue, &item, 0);
    } else if (!neighbor_entry_buff) {
      err = BmENOMEM;
    }
  }

  return err;
}

/*!
  @brief Timer handler for timeouts while waiting for neigbor tables

  @details No work is done in the timer handler, but instead an event is queued
           up to be handled in the BCMP task.

  \param tmr unused
  \return none
*/
static void topology_timer_handler(BmTimer tmr) {
  (void)tmr;

  BcmpQueueItem item = {BCMP_TOPO_EVT_TIMEOUT, NULL, NULL};

  bm_queue_send(CTX.evt_queue, &item, 0);
}

/*!
  @brief BCMP Topology Task

  @details Handles stepping through the network to request node's neighbor
           tables. Callback can be assigned/called to do something with the assembled network
           topology.

  @param parameters unused
  @return none
*/
static void bcmp_topology_thread(void *parameters) {
  (void)parameters;

  CTX.topo_timer = bm_timer_create("topology_timer", bm_ms_to_ticks(BCMP_TOPO_TIMEOUT_S * 1000),
                                   false, NULL, topology_timer_handler);

  for (;;) {
    BcmpQueueItem item;

    bm_queue_receive(CTX.evt_queue, &item, portMAX_DELAY);

    switch (item.type) {
    case BCMP_TOPO_EVT_START: {
      if (!CTX.in_progress) {
        CTX.in_progress = true;
        SENT_REQUEST = true;
        CTX.callback = item.callback;
        process_start_topology_event();
      } else {
        item.callback(NULL);
      }
      break;
    }

    case BCMP_TOPO_EVT_ADD_NODE: {
      if (item.neighbor_entry) {
        if (!network_topology_node_id_in_topology(
                CTX.network_topology, item.neighbor_entry->neighbor_table_reply->node_id)) {
          if (INSERT_BEFORE) {
            network_topology_insert_before(CTX.network_topology, item.neighbor_entry);
            network_topology_move_prev(CTX.network_topology);
          } else {
            network_topology_insert_after(CTX.network_topology, item.neighbor_entry);
            network_topology_move_next(CTX.network_topology);
          }
          network_topology_increment_port_count(
              CTX.network_topology); // we have come from one of the ports so it must have been checked
        }
      }
      BcmpQueueItem check_item = {BCMP_TOPO_EVT_CHECK_NODE, NULL, NULL};
      bm_queue_send(CTX.evt_queue, &check_item, 0);
      break;
    }

    case BCMP_TOPO_EVT_CHECK_NODE: {
      process_check_node_event();
      break;
    }

    case BCMP_TOPO_EVT_END: {
      if (CTX.callback) {
        CTX.callback(CTX.network_topology);
      }
      // Free the network topology now that we are done
      free_network_topology(&CTX.network_topology);
      INSERT_BEFORE = false;
      SENT_REQUEST = false;
      CTX.in_progress = false;
      break;
    }

    case BCMP_TOPO_EVT_TIMEOUT: {
      if (INSERT_BEFORE) {
        network_topology_move_next(CTX.network_topology);
      } else {
        network_topology_move_prev(CTX.network_topology);
      }
      BcmpQueueItem check_item = {BCMP_TOPO_EVT_CHECK_NODE, NULL, NULL};
      bm_queue_send(CTX.evt_queue, &check_item, 0);
      break;
    }

    default: {
      break;
    }
    }
  }
}

/*!
  @brief Initialize BCMP Topology Module

  @return BmOK on success
  @return BmErr on failure
*/
BmErr bcmp_topology_init(void) {
  BmErr err = BmOK;
  BcmpPacketCfg neighbor_request = {
      false,
      false,
      bcmp_process_neighbor_table_request,
  };
  BcmpPacketCfg neighbor_reply = {
      false,
      false,
      bcmp_process_neighbor_table_reply,
  };

  bm_err_check(err, packet_add(&neighbor_request, BcmpNeighborTableRequestMessage));
  bm_err_check(err, packet_add(&neighbor_reply, BcmpNeighborTableReplyMessage));
  return err;
}

BmErr bcmp_topology_start(BcmpTopoCb callback) {
  BmErr err = BmOK;

  // create the task if it is not already created
  if (!BCMP_TOPOLOGY_TASK) {
    CTX.evt_queue = bm_queue_create(BCMP_TOPO_EVT_QUEUE_LEN, sizeof(BcmpQueueItem));

    bm_err_check(err, bm_task_create(bcmp_topology_thread, "BCMP_TOPO", 1024, NULL,
                                     BCMP_TOPO_TASK_PRIORITY, &BCMP_TOPOLOGY_TASK));
  }

  // send the first request out
  BcmpQueueItem item = {BCMP_TOPO_EVT_START, NULL, callback};
  bm_err_check(err, bm_queue_send(CTX.evt_queue, &item, 0));

  return err;
}

/*!
 @brief Free a neighbor table entry that is within the network topology

 @param entry entry to be freed
 */
static void free_neighbor_table_entry(neighborTableEntry_t **entry) {

  if (entry != NULL && *entry != NULL) {
    bm_free((*entry)->neighbor_table_reply);
    bm_free(*entry);
    *entry = NULL;
  }
}
static networkTopology_t *new_network_topology(void) {
  networkTopology_t *network_topology =
      (networkTopology_t *)bm_malloc(sizeof(networkTopology_t));
  memset(network_topology, 0, sizeof(networkTopology_t));
  network_topology->index = -1;
  return network_topology;
}

static void free_network_topology(networkTopology_t **network_topology) {
  if (network_topology != NULL && *network_topology != NULL) {
    network_topology_clear(*network_topology);
    bm_free(*network_topology);
    *network_topology = NULL;
  }
}

static void network_topology_clear(networkTopology_t *network_topology) {
  if (network_topology) {
    while (network_topology->length) {
      network_topology_delete_back(network_topology);
    }
  }
}

static void network_topology_move_to_front(networkTopology_t *network_topology) {
  if (network_topology && network_topology->length) {
    network_topology->cursor = network_topology->front;
    network_topology->index = 0;
  }
}

static void network_topology_move_prev(networkTopology_t *network_topology) {
  if (network_topology) {
    if (network_topology->index > 0) {
      network_topology->cursor = network_topology->cursor->prevNode;
      network_topology->index--;
    }
  }
}

static void network_topology_move_next(networkTopology_t *network_topology) {
  if (network_topology) {
    if (network_topology->index < network_topology->length - 1) {
      network_topology->cursor = network_topology->cursor->nextNode;
      network_topology->index++;
    }
  }
}

// insert at the front
static void network_topology_prepend(networkTopology_t *network_topology,
                                     neighborTableEntry_t *entry) {
  if (network_topology && entry) {
    if (network_topology->length == 0) {
      network_topology->front = entry;
      network_topology->back = entry;
      network_topology->front->prevNode = NULL;
      network_topology->front->nextNode = NULL;
      network_topology->index++;
    } else {
      entry->nextNode = network_topology->front;
      network_topology->front->prevNode = entry;
      network_topology->front = entry;
    }
    network_topology->length++;
    if (network_topology->index >= 0) {
      network_topology->index++;
    }
  }
}

// insert at the end
static void network_topology_append(networkTopology_t *network_topology,
                                    neighborTableEntry_t *entry) {
  if (network_topology && entry) {
    if (network_topology->length == 0) {
      network_topology->front = entry;
      network_topology->back = entry;
      network_topology->front->prevNode = NULL;
      network_topology->front->nextNode = NULL;
      network_topology->index++;
    } else {
      entry->prevNode = network_topology->back;
      network_topology->back->nextNode = entry;
      network_topology->back = entry;
    }
    network_topology->length++;
  }
}

// insert after the cursor entry
static void network_topology_insert_after(networkTopology_t *network_topology,
                                          neighborTableEntry_t *entry) {
  if (network_topology && entry) {
    if (network_topology->length == 0) {
      network_topology_append(network_topology, entry);
    } else if (network_topology->index == network_topology->length - 1) {
      network_topology_append(network_topology, entry);
    } else {
      entry->nextNode = network_topology->cursor->nextNode;
      network_topology->cursor->nextNode->prevNode = entry;
      entry->prevNode = network_topology->cursor;
      network_topology->cursor->nextNode = entry;
      network_topology->length++;
    }
  }
}

// insert before the cursor entry
static void network_topology_insert_before(networkTopology_t *network_topology,
                                           neighborTableEntry_t *entry) {
  if (network_topology && entry) {
    if (network_topology->length == 0) {
      network_topology_prepend(network_topology, entry);
    } else if (network_topology->index == 0) {
      network_topology_prepend(network_topology, entry);
    } else {
      network_topology->cursor->prevNode->nextNode = entry;
      entry->nextNode = network_topology->cursor;
      entry->prevNode = network_topology->cursor->prevNode;
      network_topology->cursor->prevNode = entry;
      network_topology->length++;
      network_topology->index++;
    }
  }
}

// delete the last entry
static void network_topology_delete_back(networkTopology_t *network_topology) {
  neighborTableEntry_t *temp_entry = NULL;

  if (network_topology && network_topology->length) {
    temp_entry = network_topology->back;
    if (network_topology->length > 1) {
      network_topology->back = network_topology->back->prevNode;
      network_topology->back->nextNode = NULL;
      if (network_topology->index == network_topology->length - 1) {
        network_topology->cursor = network_topology->back;
      }
    } else {
      network_topology->cursor = NULL;
      network_topology->back = NULL;
      network_topology->front = NULL;
      network_topology->index = -1;
    }
    network_topology->length--;
    free_neighbor_table_entry(&temp_entry);
  }
}

static void network_topology_increment_port_count(networkTopology_t *network_topology) {
  if (network_topology && network_topology->cursor) {
    network_topology->cursor->ports_explored++;
  }
}

static uint8_t network_topology_get_port_count(networkTopology_t *network_topology) {
  if (network_topology && network_topology->cursor) {
    return network_topology->cursor->ports_explored;
  }
  return -1;
}

static bool network_topology_is_root(networkTopology_t *network_topology) {
  if (network_topology && network_topology->cursor) {
    return network_topology->cursor->is_root;
  }
  return false;
}

static bool network_topology_node_id_in_topology(networkTopology_t *network_topology,
                                                 uint64_t node_id) {
  neighborTableEntry_t *temp_entry;
  if (network_topology) {
    for (temp_entry = network_topology->front; temp_entry != NULL;
         temp_entry = temp_entry->nextNode) {
      if (temp_entry->neighbor_table_reply->node_id == node_id) {
        return true;
      }
    }
  }
  return false;
}

static uint64_t network_topology_check_neighbor_node_ids(networkTopology_t *network_topology) {
  if (network_topology) {
    for (uint8_t neighbors_count = 0;
         neighbors_count < network_topology->cursor->neighbor_table_reply->neighbor_len;
         neighbors_count++) {
      BcmpNeighborTableReply *temp_table = network_topology->cursor->neighbor_table_reply;
      BcmpNeighborInfo *neighbor_info =
          (BcmpNeighborInfo *)&temp_table->port_list[temp_table->port_len];
      uint64_t node_id = neighbor_info[neighbors_count].node_id;
      if (!network_topology_node_id_in_topology(network_topology, node_id)) {
        return node_id;
      }
    }
  }
  return 0;
}

static bool network_topology_check_all_ports_explored(networkTopology_t *network_topology) {
  if (network_topology) {
    uint16_t neighbors_online_count = 0;
    neighborTableEntry_t *cursor_entry = network_topology->cursor;
    uint8_t num_ports = cursor_entry->neighbor_table_reply->port_len;
    BcmpNeighborInfo *neighbor_info =
        (BcmpNeighborInfo *)&cursor_entry->neighbor_table_reply->port_list[num_ports];

    // loop through the neighbors and make sure they port is up
    for (uint16_t neighbor_count = 0;
         neighbor_count < network_topology->cursor->neighbor_table_reply->neighbor_len;
         neighbor_count++) {
      // get the port number that is associated with a neighbor
      uint8_t neighbors_port_number =
          neighbor_info[neighbor_count].port -
          1; // subtract 1 since we save the ports as 1,2,... (this means a port cannot be labeled '0')
      if (network_topology->cursor->neighbor_table_reply->port_list[neighbors_port_number]
              .state) {
        neighbors_online_count++;
      }
    }

    if (neighbors_online_count == network_topology->cursor->ports_explored) {
      return true;
    }
  }
  return false;
}

void network_topology_print(networkTopology_t *network_topology) {
  if (network_topology) {
    neighborTableEntry_t *temp_entry;
    for (temp_entry = network_topology->front; temp_entry != NULL;
         temp_entry = temp_entry->nextNode) {
      if (temp_entry->prevNode) {
        BcmpNeighborTableReply *temp_table = temp_entry->neighbor_table_reply;
        BcmpNeighborInfo *neighbor_info =
            (BcmpNeighborInfo *)&temp_table->port_list[temp_table->port_len];
        for (uint16_t neighbor_count = 0; neighbor_count < temp_table->neighbor_len;
             neighbor_count++) {
          if (temp_entry->prevNode->neighbor_table_reply->node_id ==
              neighbor_info[neighbor_count].node_id) {
            printf("%u:", neighbor_info[neighbor_count].port);
          }
        }
      }
      if (temp_entry->is_root) {
        printf("(root)%016" PRIx64 "", temp_entry->neighbor_table_reply->node_id);
      } else {
        printf("%016" PRIx64 "", temp_entry->neighbor_table_reply->node_id);
      }
      if (temp_entry->nextNode) {
        BcmpNeighborTableReply *temp_table = temp_entry->neighbor_table_reply;
        BcmpNeighborInfo *neighbor_info =
            (BcmpNeighborInfo *)&temp_table->port_list[temp_table->port_len];
        for (uint16_t neighbor_count = 0; neighbor_count < temp_table->neighbor_len;
             neighbor_count++) {
          if (temp_entry->nextNode->neighbor_table_reply->node_id ==
              neighbor_info[neighbor_count].node_id) {
            printf(":%u", neighbor_info[neighbor_count].port);
          }
        }
      }
      if (temp_entry != network_topology->back && temp_entry->nextNode != NULL) {
        printf(" | ");
      }
    }
    printf("\n");
  } else {
    printf("NULL topology, thread may be busy servicing another topology request\n");
  }
}
