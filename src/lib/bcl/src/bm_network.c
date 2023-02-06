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
            printf("Neighbor on Port %d is unresponsive\n", i);
            self_neighbor_table.neighbor[i].reachable = 0;
            break;
        }
    }
}

static void heartbeat_timer_handler(TimerHandle_t tmr){
    (void) tmr;
    static uint8_t output[128];
    size_t cbor_len;
    struct bm_Heartbeat heartbeat_msg;

    ipv6_add_padding(heartbeat_msg._bm_Heartbeat_src_ipv6_addr_uint, self_addr.addr);
    heartbeat_msg._bm_Heartbeat_src_ipv6_addr_uint_count = 16;

    bm_usr_msg_hdr_t msg_hdr = {
        .encoding = BM_ENCODING_CBOR,
        .set_id = BM_SET_ROS,
        .id = MSG_BM_HEARTBEAT,
    };

    if (cbor_encode_bm_Heartbeat(output, sizeof(output), &heartbeat_msg, &cbor_len)) {
        printf("CBOR encoding error\n");
        return;
    }

    struct pbuf* buf = pbuf_alloc(PBUF_TRANSPORT, cbor_len + sizeof(msg_hdr), PBUF_RAM);
    memcpy(buf->payload, &msg_hdr, sizeof(msg_hdr));
    memcpy(&((uint8_t *)(buf->payload))[sizeof(msg_hdr)], output, cbor_len);
    udp_sendto_if(pcb, buf, &multicast_ll_addr, port, netif);
    pbuf_free(buf);

    /* Re-start heartbeat timer */
    configASSERT(xTimerStart(tmr, 10));
}

/********************************************************************************************/
/*************************** Private CBOR Message API ***************************************/
/********************************************************************************************/

static void send_discover_neighbors(void)
{
    static uint8_t output[128];
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
        printf("CBOR encoding error\n");
        return;
    }

    struct pbuf* buf = pbuf_alloc(PBUF_TRANSPORT, cbor_len + sizeof(msg_hdr), PBUF_RAM);
    memcpy(buf->payload, &msg_hdr, sizeof(msg_hdr));
    memcpy(&((uint8_t *)(buf->payload))[sizeof(msg_hdr)], output, cbor_len);
    udp_sendto_if(pcb, buf, &multicast_ll_addr, port, netif);
    pbuf_free(buf);
}

static void send_ack(uint32_t * addr) {

    static uint8_t output[70];
    size_t cbor_len;
    struct bm_Ack ack_msg;

    ipv6_add_padding(ack_msg._bm_Ack_dst_ipv6_addr_uint, addr);
    ack_msg._bm_Ack_dst_ipv6_addr_uint_count = 16;
    ipv6_add_padding(ack_msg._bm_Ack_src_ipv6_addr_uint, self_addr.addr);
    ack_msg._bm_Ack_src_ipv6_addr_uint_count = 16;

    bm_usr_msg_hdr_t msg_hdr = {
        .encoding = BM_ENCODING_CBOR,
        .set_id = BM_SET_ROS,
        .id = MSG_BM_ACK,
    };

    if(cbor_encode_bm_Ack(output, sizeof(output), &ack_msg, &cbor_len)) {
        printf("CBOR encoding error\n");
        return;
    }

    struct pbuf* buf = pbuf_alloc(PBUF_TRANSPORT, cbor_len + sizeof(msg_hdr), PBUF_RAM);
    memcpy(buf->payload, &msg_hdr, sizeof(msg_hdr));
    memcpy(&((uint8_t *)(buf->payload))[sizeof(msg_hdr)], output, cbor_len);
    udp_sendto_if(pcb, buf, &multicast_ll_addr, port, netif);
    pbuf_free(buf);
}
/********************************************************************************************/
/*************************** Public CBOR Message API ****************************************/
/********************************************************************************************/

void bm_network_store_neighbor(uint8_t port_num, uint32_t* addr, bool is_ack) {
    if (!is_ack) {
        /* Send ack back to neighbor */
        send_ack(addr);
    }

    /* Check if we already have this Node IPV6 addr in our neighbor table */
    if (memcmp(self_neighbor_table.neighbor[port_num].ip_addr.addr, addr, 16) != 0) {
        printf("Storing Neighbor in Port Num: %d\n", port_num);
        memcpy(self_neighbor_table.neighbor[port_num].ip_addr.addr, addr, 16);
        self_neighbor_table.neighbor[port_num].reachable = 1;

        /* Kickoff Heartbeat Timer? */
        configASSERT(xTimerStart(heartbeat_timer, 10));
        
        /* Kickoff Neighbor Heartbeat Timeout Timer */
        configASSERT(xTimerStart(neighbor_timers[port_num], 10));
    } else {
        printf("Already have this node, reset its heartbeat timeout timer\n");
        self_neighbor_table.neighbor[port_num].reachable = 1;

        /* Restart neighbor heartbeat timer */
        configASSERT(xTimerStop(neighbor_timers[port_num], 10));
        configASSERT(xTimerStart(neighbor_timers[port_num], 10));
    }
}

void bm_network_heartbeat_received(uint8_t port_num, uint32_t * addr) {
    if (memcmp(self_neighbor_table.neighbor[port_num].ip_addr.addr, addr, 16) == 0) {
        printf("Valid Heartbeat received\n");
        self_neighbor_table.neighbor[port_num].reachable = 1;
        configASSERT(xTimerStop(neighbor_timers[port_num], 10));
        configASSERT(xTimerStart(neighbor_timers[port_num], 10));
    } else {
        printf("New Neighbor sending a Heartbeat. Attempting to re-discover and modify neighbor table\n");
        send_discover_neighbors();
    }
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
        configASSERT(neighbor_timers[i]);
    }

    /* TODO: Initialize timer to send out heartbeats to neighbors */
    heartbeat_timer = xTimerCreate("heartbeatTmr", (BM_HEARTBEAT_TIME_MS / portTICK_RATE_MS),
                                   pdTRUE, (void *) &tmr_id, heartbeat_timer_handler);
    configASSERT(heartbeat_timer);
    return 0;
}