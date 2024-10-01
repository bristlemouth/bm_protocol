#pragma once

#include "messages.h"
#include "util.h"
#include <stdbool.h>

#define NEIGHBOR_UUID_LEN (12)

typedef struct bm_neighbor_s {
  // Pointer to next neighbor
  struct bm_neighbor_s *next;

  uint64_t node_id;

  // Neighbor unique id
  uint8_t uuid[NEIGHBOR_UUID_LEN];

  // Which port is this neighbor on?
  uint8_t port;

  // Last time we received a message from this unit
  uint32_t last_heartbeat_ticks;

  // Last heartbeat time-since-boot. If the number is lower than the previous
  // one, the device has reset so we should request new peer information again.
  uint64_t last_time_since_boot_us;

  // Time between expected heartbeats
  uint32_t heartbeat_period_s;

  // Unit is considered online as long as heartbeats arrive on schedule
  bool online;

  // Device information
  BcmpDeviceInfo info;
  char *version_str;
  char *device_name;

  // TODO - resource list
} BcmpNeighbor;

typedef void (*NeighborCallback)(BcmpNeighbor *neighbor);
typedef void (*NeighborDiscoveryCallback)(bool discovered, BcmpNeighbor *neighbor);

BcmpNeighbor *bcmp_get_neighbors(uint8_t &num_neighbors);
void bcmp_check_neighbors();
void bcmp_print_neighbor_info(BcmpNeighbor *neighbor);
bool bcmp_remove_neighbor_from_table(BcmpNeighbor *neighbor);
bool bcmp_free_neighbor(BcmpNeighbor *neighbor);
BcmpNeighbor *bcmp_find_neighbor(uint64_t node_id);
BcmpNeighbor *bcmp_update_neighbor(uint64_t node_id, uint8_t port);
void bcmp_neighbor_foreach(NeighborCallback cb);
void bcmp_neighbor_register_discovery_callback(NeighborDiscoveryCallback cb);
