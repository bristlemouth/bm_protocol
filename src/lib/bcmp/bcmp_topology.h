#pragma once

#include "messages.h"
#include "util.h"

typedef struct neighborTableEntry_t {
  struct neighborTableEntry_t *prevNode;
  struct neighborTableEntry_t *nextNode;

  // increment this each time a port's path has been explored
  // when this is equal to the number of ports we can say
  // we have explored each ports path and can return to
  // the previous table entry to check its ports
  uint8_t ports_explored;

  bool is_root;

  BcmpNeighborTableReply *neighbor_table_reply;

} neighborTableEntry_t;

typedef struct {
  neighborTableEntry_t *front;
  neighborTableEntry_t *back;
  neighborTableEntry_t *cursor;
  uint8_t length;
  int16_t index;
} networkTopology_t;

typedef enum {
  TOPO_ASSEMBLED = 0,
  TOPO_LOADING,
  TOPO_EMPTY,
} networkTopology_status_t;

typedef void (*BcmpTopoCb)(networkTopology_t *networkTopology);

void network_topology_print(networkTopology_t *networkTopology);

// Neighbor table request defines, used to create the network topology
BmErr bcmp_request_neighbor_table(uint64_t target_node_id, const void *addr);

BmErr bcmp_topology_init(void);
// Topology task defines
BmErr bcmp_topology_start(const BcmpTopoCb callback);
