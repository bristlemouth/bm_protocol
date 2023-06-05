#pragma once

#include "lwip/ip.h"
#include "bcmp_messages.h"

#define NEIGHBOR_UUID_LEN (12)

typedef struct bm_neighbor_s {
  // Pointer to next neighbor
  struct bm_neighbor_s *next;

  // Neighbor link-local address (do we need this?)
  ip_addr_t addr;

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
  bcmp_device_info_t info;
  char *version_str;
  char *device_name;

  // TODO - resource list
} bm_neighbor_t;

bm_neighbor_t* bcmp_get_neighbors(uint8_t &num_neighbors);
void bcmp_check_neighbors();
void bcmp_print_neighbor_info(bm_neighbor_t *neighbor);
bool bcmp_remove_neighbor_from_table(bm_neighbor_t *neighbor);
bool bcmp_free_neighbor(bm_neighbor_t *neighbor);
bm_neighbor_t *bcmp_find_neighbor(uint64_t node_id);
bm_neighbor_t *bcmp_update_neighbor(uint64_t node_id, uint8_t port);
void bcmp_neighbor_foreach(void (*callback)(bm_neighbor_t *neighbor));
