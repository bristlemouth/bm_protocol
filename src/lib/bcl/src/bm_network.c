#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// Includes for FreeRTOS
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"

#include <zcbor_common.h>
#include <zcbor_decode.h>
#include <zcbor_encode.h>

#include "bm_zcbor_decode.h"
#include "bm_zcbor_encode.h"
#include "bm_usr_msg.h"
#include "bm_msg_types.h"
#include "bm_network.h"

#include "lwip/pbuf.h"
#include "lwip/inet.h"

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
static ip6_addr_t       multicast_glob_addr;
static ip6_addr_t       multicast_ll_addr;

/********************************************************************************************/
/******************************* CDDL Helper Functions **************************************/
/********************************************************************************************/

/* CDDL isn't perfect... it treats an array of 16 uint8_t (ipv6 address) as an array of 16 uint32_t. 
   lwip IPv6 treats addresses as an array of 4 uint32_t... */
   
// static void ipv6_trim_padding(uint32_t * dest_buf, uint32_t * src_buf) {
//     int i;
//     for (i = 0; i < 16; i++) {
//         dest_buf[i/4] |= ((src_buf[i] & 0xFF) << (i%4));
//     }
// }

static void ipv6_add_padding(uint32_t * dest_buf, uint32_t * src_buf) {
    int i;
    for (i = 0; i < 16; i++) {
        dest_buf[i] = (src_buf[(i/4)] >> (i%4)) & 0xFF;
    }
}

/********************************************************************************************/
/******************************* Timer Handlers *********************************************/
/********************************************************************************************/

/* Used to keep track of "reachable" neighbors */
static void neighbor_timer_handler(TimerHandle_t tmr){
    int i;
    for (i = 0; i < MAX_NUM_NEIGHBORS; i++) {
        if ( tmr == neighbor_timers[i]) {
            printf("Neighbor on Port %d is unresponsive", i);
            self_neighbor_table.neighbor[i].reachable = 0;
            break;
        }
    }
}

static void heartbeat_timer_handler(TimerHandle_t tmr){
    (void) tmr;
    /* TODO: Send out heartbeats */
}

/********************************************************************************************/
/******************************* Private Debugging Tools ************************************/
/********************************************************************************************/

// static void print_node_address(struct bm_network_node_t * node) {
//     if (node) {
//         printf("%02x%02x:%02x%02x ", (uint8_t) ((node->ip_addr.addr[3] >> 12) & 0xFF),
//                                      (uint8_t) ((node->ip_addr.addr[3] >> 8) & 0xFF),
//                                      (uint8_t) ((node->ip_addr.addr[3] >> 4) & 0xFF),
//                                      (uint8_t) (node->ip_addr.addr[3] & 0xFF));
//     }
// }

/********************************************************************************************/
/********************************************************************************************/
/********************************************************************************************/

static void send_discover_neighbors(void)
{
    uint8_t output[128];
    size_t cbor_len;
    struct bm_Discover_Neighbors discover_neighbors_msg;

    ipv6_add_padding(discover_neighbors_msg._bm_Discover_Neighbors_src_ipv6_addr_uint, self_addr.addr);
    discover_neighbors_msg._bm_Discover_Neighbors_src_ipv6_addr_uint_count = 16;

    bm_usr_msg_hdr_t msg_hdr = {
        .encoding = BM_ENCODING_CBOR,
        .set_id = BM_SET_ROS,
        .id = MSG_BM_DISCOVER_NEIGHBORS,
    };

    if (cbor_encode_bm_Discover_Neighbors(output, sizeof(output), &discover_neighbors_msg, &cbor_len)) {
        printf("CBOR encoding error");
    }

    struct pbuf* buf = pbuf_alloc(PBUF_TRANSPORT, cbor_len + sizeof(msg_hdr), PBUF_RAM);
    memcpy(buf->payload, &msg_hdr, sizeof(msg_hdr));
    memcpy(&((uint8_t *)(buf->payload))[sizeof(msg_hdr)], output, cbor_len);
    udp_sendto_if(pcb, buf, &multicast_glob_addr, port, netif);
    pbuf_free(buf);
}

/********************************************************************************************/
/******************************* Public Debugging Tools *************************************/
/********************************************************************************************/



/********************************************************************************************/
/******************************* Public BM Network API **************************************/
/********************************************************************************************/

void bm_network_request_neighbor_tables(void) {
    /* TODO: Send out request for neighbor tables over multicast */
}

void bm_network_stop(void) {
    /* TODO: Stop Heartbeat + Neighbor Timers */
}


void bm_network_start(void) {
    send_discover_neighbors();
}

int bm_network_init(ip6_addr_t _self_addr, struct udp_pcb* _pcb, uint16_t _port, struct netif* _netif) {
    int i;
    int tmr_id = 0;
    char timer_name[] = "neighborTmr ";
    uint8_t str_len = sizeof(timer_name)/sizeof(timer_name[0]);

    /* Store relevant variables from bristlemouth.c */
    self_addr = _self_addr;
    pcb = _pcb;
    port = _port;
    netif = _netif;

    inet6_aton("ff02::1", &multicast_ll_addr);
    inet6_aton("ff03::1", &multicast_glob_addr);

    /* Initialize timers for all potential neighbors */
    for (i=0; i < MAX_NUM_NEIGHBORS; i++) {
        /* Create new timer  for each potential neighbor */
        timer_name[str_len-1] = i + '0';
        neighbor_timers[i] = xTimerCreate(timer_name, (BM_NEIGHBOR_TIMEOUT_MS / portTICK_RATE_MS),
                                       pdTRUE, (void *) &tmr_id, neighbor_timer_handler);
    }

    /* TODO: Initialize timer to send out heartbeats to neighbors */
    heartbeat_timer = xTimerCreate("heartbeatTmr", (BM_HEARTBEAT_TIME_MS / portTICK_RATE_MS),
                                   pdTRUE, (void *) &tmr_id, heartbeat_timer_handler);

    return 0;
}