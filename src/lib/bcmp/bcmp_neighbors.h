#pragma once

#include "lwip/ip.h"
#include "bcmp_messages.h"

#define NEIGHBOR_UUID_LEN (12)

typedef struct bm_neigbhor_s {
  // Pointer to next neighbor
  struct bm_neigbhor_s *next;

  // Neighbor link-local address
  ip_addr_t addr;

  // TODO - do we need to keep a copy of their unicast address?

  // Neighbor unique id
  uint8_t uuid[NEIGHBOR_UUID_LEN];

  // Which port is this neighbor on?
  uint8_t port;

  // Last time we received a message from this unit
  uint32_t last_heartbeat_ticks;

  // Time between expected heartbeats
  bm_time_t heartbeat_period_us;

  // TODO - resource list
} bm_neigbhor_t;

bm_neigbhor_t *bcmp_find_neighbor(const ip_addr_t *addr);
bm_neigbhor_t *bcmp_update_neighbor(const ip_addr_t *addr, uint8_t port);
