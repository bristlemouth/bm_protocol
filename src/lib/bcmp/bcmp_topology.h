#pragma once

#include "lwip/ip.h"
#include "bcmp_messages.h"

typedef struct neighborTableEntry_t{
  struct neighborTableEntry_t* prevNode;
  struct neighborTableEntry_t* nextNode;

  // increment this each time a port's path has been explored
  // when this is equal to the number of ports we can say
  // we have explored each ports path and can return to
  // the previous table entry to check its ports
  uint8_t ports_explored;

  bool is_root;

  bcmp_neighbor_table_reply_t *neighbor_table_reply;

} neighborTableEntry_t;

typedef struct {
  neighborTableEntry_t* front;
  neighborTableEntry_t* back;
  neighborTableEntry_t* cursor;
  uint8_t length;
  int16_t index;
} networkTopology_t;

typedef enum {
  TOPO_ASSEMBLED = 0,
  TOPO_LOADING,
  TOPO_EMPTY,
} networkTopology_status_t;

typedef void (*bcmp_topo_cb_t)(networkTopology_t* networkTopology);

void networkTopologyPrint(networkTopology_t* networkTopology);

// Neighbor table request defines, used to create the network topology
err_t bcmp_request_neighbor_table(uint64_t target_node_id, const ip_addr_t *addr);
err_t bcmp_send_neighbor_table(const ip_addr_t *addr);
err_t bcmp_process_neighbor_table_request(bcmp_neighbor_table_request_t *neighbor_table_req, const ip_addr_t *src, const ip_addr_t *dst);
err_t bcmp_process_neighbor_table_reply(bcmp_neighbor_table_reply_t *neighbor_table_reply);

// Topology task defines
void bcmp_topology_start(const bcmp_topo_cb_t callback);
