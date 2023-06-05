#pragma once

#include "lwip/ip.h"
#include "bcmp_messages.h"

// Neighbor table request defines, used to create the network topology
err_t bcmp_request_neighbor_table(uint64_t target_node_id, const ip_addr_t *addr);
err_t bcmp_send_neighbor_table(const ip_addr_t *addr);
err_t bcmp_process_neighbor_table_request(bcmp_neighbor_table_request_t *neighbor_table_req, const ip_addr_t *src, const ip_addr_t *dst);
err_t bcmp_process_neighbor_table_reply(bcmp_neighbor_table_reply_t *neighbor_table_reply);

// Topology task defines
void bcmp_topology_start(void);
