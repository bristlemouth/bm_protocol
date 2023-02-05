#ifndef BM_NETWORK_H__
#define BM_NETWORK_H__

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "lwip/udp.h"
#include "lwip/ip6.h"

#define BM_NEIGHBOR_TIMEOUT_MS      15000UL /* Time to wait for a heartbeat from neighbor */
#define BM_HEARTBEAT_TIME_MS        5000UL /* Time to wait before sending heartbeat to neighbor */

/* FIXME: These shouldn't be used long-term */
#define MAX_NUM_NEIGHBORS           4
#define MAX_NUM_NETWORK_NODES       10

typedef struct bm_neighbor_info_t {
    ip6_addr_t      ip_addr;
    uint8_t         reachable;
} bm_neighbor_info_t;

typedef struct bm_neighbor_table_t {
    ip6_addr_t          ip_addr;
    bm_neighbor_info_t  neighbor[MAX_NUM_NEIGHBORS];
} bm_neighbor_table_t;

struct bm_network_node_t {
    ip6_addr_t ip_addr;
    struct bm_network_node_t* sibling;
    struct bm_network_node_t* ports[MAX_NUM_NEIGHBORS];
};

/* Network API */
int bm_network_init(ip6_addr_t _self_addr, struct udp_pcb* _pcb, uint16_t _port, struct netif* _netif);
void bm_network_start(void);
void bm_network_stop(void);
void bm_network_request_neighbor_tables(void);

void bm_network_store_neighbor(uint8_t port_num, uint32_t* addr, bool is_ack);

#endif /* BM_NETWORK_H__ */