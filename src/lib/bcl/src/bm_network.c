#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

// Includes for FreeRTOS
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"

#include "zcbor_common.h"
#include "zcbor_decode.h"
#include "zcbor_encode.h"

#include "bm_info.h"
#include "bm_caps.h"
#include "bm_msg_types.h"
#include "bm_network.h"
#include "bm_usr_msg.h"
#include "bm_util.h"

#include "lwip/pbuf.h"
#include "lwip/inet.h"
#include "safe_udp.h"

#include "task_priorities.h"

static bm_neighbor_table_t self_neighbor_table = {0};

/* Timers */
static TimerHandle_t neighbor_timers[MAX_NUM_NEIGHBORS];
static TimerHandle_t heartbeat_timer;

/* TODO: Create a local heap to allocate memory from instead of large memory bank
K_HEAP_DEFINE(network_pool, sizeof(bm_network_node_t * MAX_NUM_NETWORK_NODES);
*/

static ip6_addr_t       self_addr;
static struct netif*    netif;
static struct udp_pcb*  pcb;
static uint16_t         port;

/*  Linked List Mapping of Network Nodes
    Currently using stack memory but can eventually switch to using heap */
static struct bm_network_node_t network_map[MAX_NUM_NETWORK_NODES];
static uint8_t network_map_idx = 0;

#define HEARTBEAT_ENCODE_BUF_LEN            128
#define DISCOVER_NEIGHBOR_ENCODE_BUF_LEN    128
#define ACK_ENCODE_BUF_LEN                  70
#define TABLE_RESPONSE_ENCODE_BUF_LEN       256
#define REQUEST_TABLE_ENCODE_BUF_LEN        128

/********************************************************************************************/
/******************************* Timer Handlers *********************************************/
/********************************************************************************************/

// Callback function
typedef struct {
    void (*fn)(void *arg);
    void *arg;
} __attribute__((packed)) timerCallback_t;

#define TIMER_QUEUE_SIZE 8
static xQueueHandle timerEventQueue;

static void _print_unresponsive_neighbor(void *arg) {
    uint32_t neighbor = (uint32_t)arg;
    printf("Neighbor on Port %u is unresponsive\n", (uint8_t)neighbor);
}

/* Used to keep track of "reachable" neighbors */
static void neighbor_timer_handler(TimerHandle_t tmr) {
    for (uint32_t neighbor = 0; neighbor < MAX_NUM_NEIGHBORS; neighbor++) {
        if ( tmr == neighbor_timers[neighbor]) {
            // Stop the timer
            configASSERT(xTimerStop(tmr, 0));

            // Update table
            self_neighbor_table.neighbor[neighbor].reachable = 0;

            // Print message (outside timer callback context)
            timerCallback_t callback = {_print_unresponsive_neighbor, (void*)neighbor};
            configASSERT(xQueueSend( timerEventQueue, &callback, 0 ));
            break;
        }
    }
}

static void _send_heartbeat(void *arg) {
    (void)arg;

    size_t cbor_len;
    struct bm_Heartbeat heartbeat_msg;

    memcpy(heartbeat_msg._bm_Heartbeat_src_ipv6_addr_uint, self_addr.addr, sizeof(heartbeat_msg._bm_Heartbeat_src_ipv6_addr_uint));
    heartbeat_msg._bm_Heartbeat_src_ipv6_addr_uint_count = 4;

    bm_usr_msg_hdr_t msg_hdr = {
        .encoding = BM_ENCODING_CBOR,
        .set_id = BM_SET_ROS,
        .id = MSG_BM_HEARTBEAT,
    };

    uint8_t* output = (uint8_t *) pvPortMalloc(HEARTBEAT_ENCODE_BUF_LEN);
    configASSERT(output);

    do {
        if (cbor_encode_bm_Heartbeat(output, HEARTBEAT_ENCODE_BUF_LEN, &heartbeat_msg, &cbor_len)) {
            printf("CBOR encoding error\n");
            break;
        }

        struct pbuf* buf = pbuf_alloc(PBUF_TRANSPORT, cbor_len + sizeof(msg_hdr), PBUF_RAM);
        configASSERT(buf);
        memcpy(buf->payload, &msg_hdr, sizeof(msg_hdr));
        memcpy(&((uint8_t *)(buf->payload))[sizeof(msg_hdr)], output, cbor_len);
        safe_udp_sendto_if(pcb, buf, &multicast_ll_addr, port, netif);
        pbuf_free(buf);
    } while (0);
    vPortFree(output);
}

static void heartbeat_timer_handler(TimerHandle_t tmr){
    (void) tmr;

    // Send heartbeat (happens outside timer handler context)
    timerCallback_t callback = {_send_heartbeat, NULL};
    configASSERT(xQueueSend( timerEventQueue, &callback, 0 ));

    /* Re-start heartbeat timer */
    configASSERT(xTimerStart(tmr, 0));
}

/********************************************************************************************/
/******************************* Helper Functions *******************************************/
/********************************************************************************************/

/* Search for IPv6 and return index */
static int8_t get_node_index (ip6_addr_t* ip_addr) {
    int8_t retval = -1;

    for (int node=0; node<network_map_idx+1; node++) {
        if (memcmp(network_map[node].ip_addr.addr, ip_addr->addr, sizeof(network_map[node].ip_addr.addr)) == 0) {
            retval = node;
            break;
        }
    }

    return retval;
}

/********************************************************************************************/
/*************************** Private CBOR Message API ***************************************/
/********************************************************************************************/

static void send_discover_neighbors(void)
{
    size_t cbor_len;
    struct bm_Discover_Neighbors discover_neighbors_msg;

    uint8_t* output = (uint8_t *) pvPortMalloc(DISCOVER_NEIGHBOR_ENCODE_BUF_LEN);
    configASSERT(output);

    memcpy(discover_neighbors_msg._bm_Discover_Neighbors_src_ipv6_addr_uint, self_addr.addr, sizeof(self_addr.addr));
    discover_neighbors_msg._bm_Discover_Neighbors_src_ipv6_addr_uint_count = 4;

    bm_usr_msg_hdr_t msg_hdr = {
        .encoding = BM_ENCODING_CBOR,
        .set_id = BM_SET_ROS,
        .id = MSG_BM_DISCOVER_NEIGHBORS,
    };

    do {
        if (cbor_encode_bm_Discover_Neighbors(output, DISCOVER_NEIGHBOR_ENCODE_BUF_LEN, &discover_neighbors_msg, &cbor_len)) {
            printf("CBOR encoding error\n");
            break;
        }

        struct pbuf* buf = pbuf_alloc(PBUF_TRANSPORT, cbor_len + sizeof(msg_hdr), PBUF_RAM);
        memcpy(buf->payload, &msg_hdr, sizeof(msg_hdr));
        memcpy(&((uint8_t *)(buf->payload))[sizeof(msg_hdr)], output, cbor_len);
        safe_udp_sendto_if(pcb, buf, &multicast_ll_addr, port, netif);
        pbuf_free(buf);
    } while(0);
    vPortFree(output);
}

static void send_ack(uint32_t * addr) {
    size_t cbor_len;
    struct bm_Ack ack_msg;

    memcpy(ack_msg._bm_Ack_dst_ipv6_addr_uint, addr, sizeof(ack_msg._bm_Ack_dst_ipv6_addr_uint));
    ack_msg._bm_Ack_dst_ipv6_addr_uint_count = 4;
    memcpy(ack_msg._bm_Ack_src_ipv6_addr_uint, self_addr.addr, sizeof(ack_msg._bm_Ack_src_ipv6_addr_uint));
    ack_msg._bm_Ack_src_ipv6_addr_uint_count = 4;

    bm_usr_msg_hdr_t msg_hdr = {
        .encoding = BM_ENCODING_CBOR,
        .set_id = BM_SET_ROS,
        .id = MSG_BM_ACK,
    };

    uint8_t* output = (uint8_t *) pvPortMalloc(ACK_ENCODE_BUF_LEN);
    configASSERT(output);

    do {
        if(cbor_encode_bm_Ack(output, ACK_ENCODE_BUF_LEN, &ack_msg, &cbor_len)) {
            printf("CBOR encoding error\n");
            break;
        }

        struct pbuf* buf = pbuf_alloc(PBUF_TRANSPORT, cbor_len + sizeof(msg_hdr), PBUF_RAM);
        memcpy(buf->payload, &msg_hdr, sizeof(msg_hdr));
        memcpy(&((uint8_t *)(buf->payload))[sizeof(msg_hdr)], output, cbor_len);
        safe_udp_sendto_if(pcb, buf, &multicast_ll_addr, port, netif);
        pbuf_free(buf);
    } while(0);
    vPortFree(output);
}
/********************************************************************************************/
/*************************** Public CBOR Message API ****************************************/
/********************************************************************************************/

void bm_network_store_neighbor(uint8_t port_num, uint32_t* addr, bool is_ack) {
    if (!is_ack) {
        /* Send ack back to neighbor */
        send_ack(addr);
    }

    // printf("Storing neighbor in port: %d -- ACK: %d\n", port_num, is_ack);

    /* Check if we already have this Node IPV6 addr in our neighbor table */
    if (memcmp(self_neighbor_table.neighbor[port_num].ip_addr.addr, addr, sizeof(self_neighbor_table.neighbor[port_num].ip_addr.addr)) != 0) {
        memcpy(self_neighbor_table.neighbor[port_num].ip_addr.addr, addr, sizeof(self_neighbor_table.neighbor[port_num].ip_addr.addr));
        self_neighbor_table.neighbor[port_num].reachable = 1;

        /* Kickoff Neighbor Heartbeat Timeout Timer */
        configASSERT(xTimerStart(neighbor_timers[port_num], 10));
    } else {
        self_neighbor_table.neighbor[port_num].reachable = 1;

        /* Restart neighbor heartbeat timer */
        configASSERT(xTimerStart(neighbor_timers[port_num], 10));
    }
}

void bm_network_heartbeat_received(uint8_t port_num, uint32_t * addr) {
    if (memcmp(self_neighbor_table.neighbor[port_num].ip_addr.addr, addr, sizeof(self_neighbor_table.neighbor[port_num].ip_addr.addr)) == 0) {
        self_neighbor_table.neighbor[port_num].reachable = 1;
        configASSERT(xTimerStart(neighbor_timers[port_num], 10));
    } else {
        send_discover_neighbors();
    }
}

void bm_network_process_table_request(uint32_t* addr) {
    size_t cbor_len;
    struct bm_Table_Response response_msg;

    /* Store Self IPv6 */
    memcpy(response_msg._bm_Table_Response_src_ipv6_addr_uint, self_addr.addr, sizeof(self_addr.addr));
    response_msg._bm_Table_Response_src_ipv6_addr_uint_count = 4;

    /* Store Neighbors */
    for (int neighbor=0; neighbor < MAX_NUM_NEIGHBORS; neighbor++) {
        if (self_neighbor_table.neighbor[neighbor].reachable) {
            memcpy(response_msg._bm_Table_Response_neighbors__bm_Neighbor_Info[neighbor]._bm_Neighbor_Info_ipv6_addr_uint, self_neighbor_table.neighbor[neighbor].ip_addr.addr, sizeof(self_neighbor_table.neighbor[neighbor].ip_addr.addr));
            response_msg._bm_Table_Response_neighbors__bm_Neighbor_Info[neighbor]._bm_Neighbor_Info_ipv6_addr_uint_count = 4;
            response_msg._bm_Table_Response_neighbors__bm_Neighbor_Info[neighbor]._bm_Neighbor_Info_reachable = self_neighbor_table.neighbor[neighbor].reachable;
            response_msg._bm_Table_Response_neighbors__bm_Neighbor_Info[neighbor]._bm_Neighbor_Info_port = neighbor;
        }
        else {
            memset(&response_msg._bm_Table_Response_neighbors__bm_Neighbor_Info[neighbor], 0, sizeof(struct bm_Neighbor_Info));
        }
    }
    response_msg._bm_Table_Response_neighbors__bm_Neighbor_Info_count = MAX_NUM_NEIGHBORS;

    /* Store Destination IPv6 (already in uint32_t array format so no conversion needed ) */
    memcpy(response_msg._bm_Table_Response_dst_ipv6_addr_uint, addr, sizeof(response_msg._bm_Table_Response_dst_ipv6_addr_uint));
    response_msg._bm_Table_Response_dst_ipv6_addr_uint_count = 4;

    bm_usr_msg_hdr_t msg_hdr = {
        .encoding = BM_ENCODING_CBOR,
        .set_id = BM_SET_ROS,
        .id = MSG_BM_TABLE_RESPONSE,
    };

    uint8_t* output = (uint8_t *) pvPortMalloc(TABLE_RESPONSE_ENCODE_BUF_LEN);
    configASSERT(output);

    do {
        if(cbor_encode_bm_Table_Response(output, TABLE_RESPONSE_ENCODE_BUF_LEN, &response_msg, &cbor_len)) {
            printf("CBOR encoding error\n");
            break;
        }

        struct pbuf* buf = pbuf_alloc(PBUF_TRANSPORT, cbor_len + sizeof(msg_hdr), PBUF_RAM);
        memcpy(buf->payload, &msg_hdr, sizeof(msg_hdr));
        memcpy(&((uint8_t *)(buf->payload))[sizeof(msg_hdr)], output, cbor_len);
        safe_udp_sendto_if(pcb, buf, &multicast_global_addr, port, netif);
        pbuf_free(buf);
    } while (0);
    vPortFree(output);
}

void bm_network_store_neighbor_table(struct bm_Table_Response* table_resp, uint32_t* ip_addr) {
    bm_neighbor_table_t peer_nt;
    int8_t incoming_node_idx;
    int8_t neighbor_idx;

    /* Store device's IPv6 */
    memcpy(peer_nt.ip_addr.addr, ip_addr, sizeof(peer_nt.ip_addr.addr));

    /* Populate the peer neighbor table struct */
    for(int neighbor=0; neighbor< MAX_NUM_NEIGHBORS; neighbor++) {
        memcpy(peer_nt.neighbor[neighbor].ip_addr.addr, table_resp->_bm_Table_Response_neighbors__bm_Neighbor_Info[neighbor]._bm_Neighbor_Info_ipv6_addr_uint, sizeof(peer_nt.neighbor[neighbor].ip_addr.addr));
        peer_nt.neighbor[neighbor].reachable = table_resp->_bm_Table_Response_neighbors__bm_Neighbor_Info[neighbor]._bm_Neighbor_Info_reachable;
    }

    /* Check if Node is already in map */
    incoming_node_idx = get_node_index(&peer_nt.ip_addr);
    configASSERT(incoming_node_idx >= 0);

    /* Node in map */
    if (incoming_node_idx >= 0) {
        // printf("Node already exists in map!\n");
        /* Iterate through the Node's N neighbors */
        for(int neighbor=0; neighbor<MAX_NUM_NEIGHBORS; neighbor++) {
            /* Check if the neighbor is reachable */
            if(peer_nt.neighbor[neighbor].reachable) {
                /* Try grabbing index of neighbor */
                neighbor_idx = get_node_index(&peer_nt.neighbor[neighbor].ip_addr);

                /* Neighbor Node exists already! Point to it */
                if (neighbor_idx >= 0) {
                    // printf("Node Neighbor Exists #1!\n");
                    network_map[incoming_node_idx].ports[neighbor] = &network_map[neighbor_idx];
                }
                /* Neighbor Node doesn't exist */
                else {
                    // printf("Adding New Node Neighbor #1!\n");
                    /* Populate next node */
                    network_map_idx++;
                    memcpy(network_map[network_map_idx].ip_addr.addr, peer_nt.neighbor[neighbor].ip_addr.addr, sizeof(peer_nt.neighbor[neighbor].ip_addr.addr));
                    network_map[incoming_node_idx].ports[neighbor] = &network_map[network_map_idx];
                }
            }
        }
    }
    /* IPv6 address not currently in network map */
    else {
        // printf("Node does not exist in map\n");
        /* Populate next Node */
        network_map_idx++;
        memcpy(network_map[network_map_idx].ip_addr.addr, peer_nt.ip_addr.addr, sizeof(peer_nt.ip_addr.addr));
        incoming_node_idx = network_map_idx;

        for(int neighbor=0; neighbor<MAX_NUM_NEIGHBORS; neighbor++) {
            if(peer_nt.neighbor[neighbor].reachable) {
                neighbor_idx = get_node_index(&peer_nt.neighbor[neighbor].ip_addr);

                /* Neighbor Node exists already! Point to it */
                if (neighbor_idx >= 0) {
                    // printf("Node Neighbor Exists #2!\n");
                    network_map[incoming_node_idx].ports[neighbor] = &network_map[neighbor_idx];
                }
                /* Neighbor Node doesn't exist */
                else {
                    // printf("Adding New Node Neighbor #2!\n");
                    /* Populate next node */
                    network_map_idx++;
                    memcpy(network_map[network_map_idx].ip_addr.addr, peer_nt.neighbor[neighbor].ip_addr.addr, sizeof(peer_nt.neighbor[neighbor].ip_addr.addr));
                    network_map[incoming_node_idx].ports[neighbor] = &network_map[network_map_idx];
                }
            }
        }
    }
}

/********************************************************************************************/
/******************************* Public BM Network API **************************************/
/********************************************************************************************/

/* Send out request for neighbor tables over multicast */
void bm_network_request_neighbor_tables(void) {
    size_t cbor_len;

    /* Set map index back to 0 */
    network_map_idx = 0;

    uint8_t root_idx = network_map_idx;
    memcpy(network_map[root_idx].ip_addr.addr, self_addr.addr, sizeof(self_addr.addr));

    for (int neighbor = 0; neighbor < MAX_NUM_NEIGHBORS; neighbor++) {
        if (self_neighbor_table.neighbor[neighbor].reachable) {
            network_map_idx++;
            memcpy(network_map[network_map_idx].ip_addr.addr, self_neighbor_table.neighbor[neighbor].ip_addr.addr, sizeof(self_neighbor_table.neighbor[neighbor].ip_addr.addr));
            network_map[root_idx].ports[neighbor] = &network_map[network_map_idx];
        }
    }

    struct bm_Request_Table request_msg;
    uint8_t* output = (uint8_t *) pvPortMalloc(REQUEST_TABLE_ENCODE_BUF_LEN);
    configASSERT(output);

    memcpy(request_msg._bm_Request_Table_src_ipv6_addr_uint, self_addr.addr, sizeof(self_addr.addr));
    request_msg._bm_Request_Table_src_ipv6_addr_uint_count = 4;

    bm_usr_msg_hdr_t msg_hdr = {
        .encoding = BM_ENCODING_CBOR,
        .set_id = BM_SET_ROS,
        .id = MSG_BM_REQUEST_TABLE,
    };

    do {
        if(cbor_encode_bm_Request_Table(output, REQUEST_TABLE_ENCODE_BUF_LEN, &request_msg, &cbor_len)) {
            printf("CBOR encoding error\n");
            break;
        }

        struct pbuf* buf = pbuf_alloc(PBUF_TRANSPORT, cbor_len + sizeof(msg_hdr), PBUF_RAM);
        memcpy(buf->payload, &msg_hdr, sizeof(msg_hdr));
        memcpy(&((uint8_t *)(buf->payload))[sizeof(msg_hdr)], output, cbor_len);
        safe_udp_sendto_if(pcb, buf, &multicast_global_addr, port, netif);
        pbuf_free(buf);
    } while(0);
    vPortFree(output);
}

void bm_network_print_neighbor_table(void) {
    printf("Neighbor Table\n");
    for (int neighbor=0; neighbor < MAX_NUM_NEIGHBORS; neighbor++)
    {
        printf("Port %d | ", neighbor);
        printf("IPv6 Address: %s | ", ip6addr_ntoa(&self_neighbor_table.neighbor[neighbor].ip_addr));
        printf("Reachable: %d\n", (self_neighbor_table.neighbor[neighbor].reachable == 1));
    }
}

/* Recursive Method of iterating through and printing Network Topology */
void bm_network_print_topology(struct bm_network_node_t* node, struct bm_network_node_t *prev, uint8_t space_count) {
    uint8_t num_children;
    struct bm_network_node_t* curr_node;
    char * curr_node_str;

    if (node == NULL) {
        curr_node = &network_map[0];
    } else {
        curr_node = node;
    }
    curr_node_str = ip6addr_ntoa(&curr_node->ip_addr);
    if (prev) {
        for(int neighbor=0; neighbor<MAX_NUM_NEIGHBORS; neighbor++) {
            if (curr_node->ports[neighbor]) {
                if ((memcmp(prev->ip_addr.addr, curr_node->ports[neighbor]->ip_addr.addr, sizeof(curr_node->ports[neighbor]->ip_addr.addr)) == 0)) {
                    printf("%d : ", neighbor);
                    space_count += 4; // Adding size of "%d : "
                    break;
                }
            }
        }
        printf("%s", curr_node_str);
        space_count += strlen(curr_node_str);
    } else {
        printf("Root ");
        printf("%s", curr_node_str);
        space_count += strlen(curr_node_str) + 5; // Adding size of "Root "
    }

    num_children = 0;
    for(int neighbor=0; neighbor<MAX_NUM_NEIGHBORS; neighbor++) {
        if (curr_node->ports[neighbor]) {
            if (prev == NULL || (memcmp(prev->ip_addr.addr, curr_node->ports[neighbor]->ip_addr.addr, sizeof(curr_node->ports[neighbor]->ip_addr.addr)) != 0)) {
                printf(" : %d | ", neighbor );
                num_children++;
                bm_network_print_topology(curr_node->ports[neighbor], curr_node, space_count + 7); // Adding size of " : %d | "

                for (int space=0; space < space_count; space++) {
                    printf(" ");
                }
            }
        }
    }
    if (num_children == 0) {
        printf("\n");
    }
}

// Keep a copy of the address we're expecting fw info from
// since we might get it from two different units due to lack
// of unicast messages
static ip_addr_t info_addr;

/*!
  Send firmware info request

  \param[in] *addr - unused until multicast works
  \return None
*/
void bm_network_request_fw_info(const ip_addr_t *addr) {
    bm_usr_msg_hdr_t msg_hdr = {
        .encoding = BM_ENCODING_CBOR,
        .set_id = BM_SET_DEFAULT,
        .id = MSG_BM_REQUEST_FW_INFO,
    };

    struct pbuf* buf = pbuf_alloc(PBUF_TRANSPORT, sizeof(msg_hdr), PBUF_RAM);
    configASSERT(buf);
    memcpy(buf->payload, &msg_hdr, sizeof(msg_hdr));

    if(safe_udp_sendto_if(pcb, buf, &multicast_global_addr, port, netif) == ERR_OK) {
        ip_addr_copy(info_addr, *addr);
    }
    pbuf_free(buf);
}

/*!
  Get address we're expecting firmware info from
  \return address
*/
ip_addr_t *bm_network_get_fw_info_ip() {
    return &info_addr;
}

#define DEV_INFO_BUf_LEN 1024
/*!
  Send firmware information

  \param[in] *addr - unused until multicast works
  \return None
*/
void bm_network_send_fw_info(const ip_addr_t * addr) {
    bm_usr_msg_hdr_t msg_hdr = {
        .encoding = BM_ENCODING_CBOR,
        .set_id = BM_SET_DEFAULT,
        .id = MSG_BM_FW_INFO,
    };

    // Unicast isn't working right now :'(
    (void)addr;

    uint8_t* payload = (uint8_t *) pvPortMalloc(DEV_INFO_BUf_LEN);
    configASSERT(payload);

    do {
        uint32_t payload_len = DEV_INFO_BUf_LEN;
        if(!bm_info_get_cbor(payload, &payload_len)) {
            printf("Error encoding device info\n");
            break;
        }

        struct pbuf* buf = pbuf_alloc(PBUF_TRANSPORT, sizeof(msg_hdr) + payload_len, PBUF_RAM);
        configASSERT(buf);
        memcpy(buf->payload, &msg_hdr, sizeof(msg_hdr));
        memcpy(&((uint8_t *)(buf->payload))[sizeof(msg_hdr)], payload, payload_len);

        safe_udp_sendto_if(pcb, buf, &multicast_global_addr, port, netif);
        pbuf_free(buf);
    } while(0);
    vPortFree(payload);
}

#define DEV_CAPS_BUF_LEN 1024

/*!
  Send Pub/Sub Capabilites  request

  \param[in] *addr - unused until multicast works
  \return None
*/
void bm_network_request_caps(void) {
    bm_usr_msg_hdr_t msg_hdr = {
        .encoding = BM_ENCODING_CBOR,
        .set_id = BM_SET_DEFAULT,
        .id = MSG_BM_REQUEST_CAPS,
    };

    struct pbuf* buf = pbuf_alloc(PBUF_TRANSPORT, sizeof(msg_hdr), PBUF_RAM);
    configASSERT(buf);
    memcpy(buf->payload, &msg_hdr, sizeof(msg_hdr));

    safe_udp_sendto_if(pcb, buf, &multicast_global_addr, port, netif);
    pbuf_free(buf);
}

/*!
  Send pubsub capabilities

  \param[in] *addr - unused until multicast works
  \return None
*/
void bm_network_send_caps(const ip_addr_t * addr) {
    bm_usr_msg_hdr_t msg_hdr = {
        .encoding = BM_ENCODING_CBOR,
        .set_id = BM_SET_DEFAULT,
        .id = MSG_BM_CAPS,
    };

    // Unicast isn't working right now :'(
    (void)addr;

    uint8_t* payload = (uint8_t *) pvPortMalloc(DEV_CAPS_BUF_LEN);
    configASSERT(payload);

    do {
        uint32_t payload_len = DEV_CAPS_BUF_LEN;
        if(!bm_caps_get_cbor(payload, &payload_len)) {
            printf("Error encoding device info\n");
            break;
        }

        struct pbuf* buf = pbuf_alloc(PBUF_TRANSPORT, sizeof(msg_hdr) + payload_len, PBUF_RAM);
        configASSERT(buf);
        memcpy(buf->payload, &msg_hdr, sizeof(msg_hdr));
        memcpy(&((uint8_t *)(buf->payload))[sizeof(msg_hdr)], payload, payload_len);

        safe_udp_sendto_if(pcb, buf, &multicast_global_addr, port, netif);
        pbuf_free(buf);
    } while(0);
    vPortFree(payload);
}

void bm_network_stop(void) {
    /* TODO: Stop Heartbeat + Neighbor Timers */
}

void bm_network_start(void) {
    send_discover_neighbors();
}

//
// Thread to handle heartbeat timer callbacks
// TODO - make this generic
//
static void bm_network_timer_thread(void *parameters) {
    (void) parameters;

    timerEventQueue = xQueueCreate(TIMER_QUEUE_SIZE, sizeof(timerCallback_t));
    configASSERT(timerEventQueue != NULL);

    int tmr_id = 0;
    char timer_name[] = "neighborTmr ";
    uint8_t str_len = sizeof(timer_name)/sizeof(timer_name[0]);

    /* Initialize timers for all potential neighbors */
    for (int neighbor=0; neighbor < MAX_NUM_NEIGHBORS; neighbor++) {
        /* Create new timer  for each potential neighbor */
        timer_name[str_len-1] = neighbor + '0';
        neighbor_timers[neighbor] = xTimerCreate(timer_name, (BM_NEIGHBOR_TIMEOUT_MS / portTICK_RATE_MS),
                                       pdTRUE, (void *) &tmr_id, neighbor_timer_handler);
        configASSERT(neighbor_timers[neighbor]);
    }

    /* Initialize timer to send out heartbeats to neighbors */
    heartbeat_timer = xTimerCreate("heartbeatTmr", (BM_HEARTBEAT_TIME_MS / portTICK_RATE_MS),
                                   pdTRUE, (void *) &tmr_id, heartbeat_timer_handler);
    configASSERT(heartbeat_timer);
    configASSERT(xTimerStart(heartbeat_timer, 10));

    for(;;) {
        timerCallback_t callback;
        BaseType_t rval = xQueueReceive(timerEventQueue, &callback, portMAX_DELAY);
        configASSERT(rval == pdTRUE);
        if(callback.fn) {
            callback.fn(callback.arg);
        }
    }
}

int bm_network_init(struct udp_pcb* _pcb, uint16_t _port, struct netif* _netif) {
    /* Store relevant variables from bristlemouth.c */
    configASSERT(_netif);
    configASSERT(_pcb);

    self_addr = *netif_ip6_addr(_netif, 0);
    pcb = _pcb;
    port = _port;
    netif = _netif;

    // TODO - Create a single timer handler thread for project
    // xTimer callbacks MUST NOT do any sort of sleeps/delays, so
    // we need to handle the callbacks in a regular task context
    BaseType_t rval = xTaskCreate(bm_network_timer_thread,
                       "BM Net Timers",
                       2048,
                       NULL,
                       BM_NETWORK_TIMER_TASK_PRIORITY,
                       NULL);
    configASSERT(rval == pdPASS);


    return 0;
}
