#include "bristlemouth.h"

// Includes for FreeRTOS
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "device_info.h"

#include "lwip/init.h"
#include "lwip/tcpip.h"
#include "lwip/udp.h"
#include "lwip/inet.h"
#include "lwip/mld6.h"

#include "zcbor_common.h"
#include "zcbor_decode.h"
#include "zcbor_encode.h"

#include "bm_dfu.h"
#include "bm_info.h"
#include "bm_caps.h"
#include "bm_l2.h"
#include "bm_msg_types.h"
#include "bm_network.h"
#include "bm_ports.h"
#include "bm_usr_msg.h"
#include "bm_zcbor_decode.h"
#include "bm_zcbor_encode.h"

#include "middleware.h"
#include "task_priorities.h"

static struct netif     netif;
static struct udp_pcb*  udp_pcb;
static uint16_t         udp_port;
static TaskHandle_t     rx_thread = NULL;
static QueueHandle_t    bcl_rx_queue;

/* Ingress and Egress ports are mapped to the 5th and 6th byte of the IPv6 src address as per
    the bristlemouth protocol spec */
#define CLEAR_PORTS(x) (x[IPV6_ADDR_DWORD_1] &= (~(0xFFFFU)))

/* Define here for now */
#define BCL_RX_QUEUE_NUM_ENTRIES  5

static void bcl_rx_cb(void *arg, struct udp_pcb *upcb, struct pbuf *buf,
                 const ip_addr_t *addr, u16_t port) {
    (void) arg;
    (void) upcb;
    (void) addr;
    (void) port;
    static bcl_rx_element_t rx_data;

    if (buf != NULL) {
        rx_data.buf = buf;
        rx_data.src = *addr;
        pbuf_ref(buf);
        if(xQueueSend(bcl_rx_queue, &rx_data, 0) != pdTRUE) {
            pbuf_free(buf);
            printf("Error sending to Queue\n");
        }
        pbuf_free(buf);
    }
}

static void bcl_rx_thread(void *parameters) {
    (void) parameters;
    static bcl_rx_element_t rx_data;
    uint16_t payload_length = 0;
    size_t decode_len;
    uint8_t dst_port_bitmask = 0;
    uint8_t dst_port_num = 0;
    // uint8_t src_port_bitmask = 0;
    // uint8_t src_port_num = 0;
    const ip6_addr_t* self_addr = netif_ip6_addr(&netif, 0);

    /* Available message types */
    struct bm_Ack ack_msg;
    struct bm_Discover_Neighbors bm_discover_neighbors_msg;
    struct bm_Heartbeat heartbeat_msg;
    struct bm_Request_Table bm_request_table_msg;
    struct bm_Table_Response bm_table_response_msg;
    int index;

    while (1) {
        if(xQueueReceive(bcl_rx_queue, &rx_data, portMAX_DELAY) == pdPASS) {

            /* Ingress and Egress ports are mapped to the 5th and 6th byte of the IPv6 src address as per
               the bristlemouth protocol spec */
            //src_port_bitmask = ((rx_data.src.addr[IPV6_ADDR_DWORD_1]) & 0xFF);
            dst_port_bitmask = ((rx_data.src.addr[IPV6_ADDR_DWORD_1] >> 8) & 0xFF);

            dst_port_num = 0xFF;
            for (index=0; index < 8; index++) {
                if (dst_port_bitmask & (0x1 << index)){
                    dst_port_num = index;
                    break;
                }
            }
            configASSERT(dst_port_num != 0xFF);

            /* Remove the ingress/egress ports before storing in neighbor table */
            CLEAR_PORTS(rx_data.src.addr);

            if (rx_data.buf->len < sizeof(bm_usr_msg_hdr_t)) {
                printf("Received a message too small to decode. Discarding.\n");
                /* free the pbuf */
                pbuf_free(rx_data.buf);
                continue;
            }

            if (rx_data.buf != NULL) {
                payload_length = rx_data.buf->len - sizeof(bm_usr_msg_hdr_t);
                /* We assume that a bm_usr_msg_hdr_t precedes the payload.
                   Inspect the message type ID */

                switch (((bm_usr_msg_hdr_t* ) rx_data.buf->payload)->id) {
                    case MSG_BM_ACK: {
                        if(cbor_decode_bm_Ack(&(((uint8_t *)(rx_data.buf->payload))[sizeof(bm_usr_msg_hdr_t)]), payload_length, &ack_msg, &decode_len)) {
                            printf("CBOR Decode error\n");
                            break;
                        }
                        bm_network_store_neighbor(dst_port_num, rx_data.src.addr, true);
                        break;
                    }
                    case MSG_BM_HEARTBEAT: {
                        if(cbor_decode_bm_Heartbeat(&(((uint8_t *)(rx_data.buf->payload))[sizeof(bm_usr_msg_hdr_t)]), payload_length, &heartbeat_msg, &decode_len)) {
                            printf("CBOR Decode error\n");
                            break;
                        }
                        bm_network_heartbeat_received(dst_port_num, rx_data.src.addr);
                        break;
                    }
                    case MSG_BM_DISCOVER_NEIGHBORS: {
                        if(cbor_decode_bm_Discover_Neighbors(&(((uint8_t *)(rx_data.buf->payload))[sizeof(bm_usr_msg_hdr_t)]), payload_length, &bm_discover_neighbors_msg, &decode_len)) {
                            printf("CBOR Decode error\n");
                            break;
                        }
                        bm_network_store_neighbor(dst_port_num, rx_data.src.addr, false);
                        break;
                    }
                    case MSG_BM_DURATION: {
                        printf("Received Duration\n");
                        break;
                    }
                    case MSG_BM_TIME: {
                        printf("Received Time\n");
                        break;
                    }
                    case MSG_BM_REQUEST_TABLE: {
                        if(cbor_decode_bm_Request_Table(&(((uint8_t *)(rx_data.buf->payload))[sizeof(bm_usr_msg_hdr_t)]), payload_length, &bm_request_table_msg, &decode_len)) {
                            printf("CBOR Decode error\n");
                            break;
                        }
                        bm_network_process_table_request(bm_request_table_msg._bm_Request_Table_src_ipv6_addr_uint);
                        break;
                    }
                    case MSG_BM_TABLE_RESPONSE: {
                        if(cbor_decode_bm_Table_Response(&(((uint8_t *)(rx_data.buf->payload))[sizeof(bm_usr_msg_hdr_t)]), payload_length, &bm_table_response_msg, &decode_len)) {
                            printf("CBOR Decode error\n");
                            break;
                        }
                        if (memcmp(bm_table_response_msg._bm_Table_Response_dst_ipv6_addr_uint, self_addr->addr, sizeof(self_addr->addr)) == 0) {
                            bm_network_store_neighbor_table(&bm_table_response_msg, bm_table_response_msg._bm_Table_Response_src_ipv6_addr_uint);
                        }
                        break;
                    }
                    case MSG_BM_REQUEST_FW_INFO: {
                        bm_network_send_fw_info(&rx_data.src);
                        break;
                    }
                    case MSG_BM_FW_INFO: {
                        ip_addr_t *info_addr = bm_network_get_fw_info_ip();
                        if(ip_addr_eq(info_addr, &rx_data.src)) {
                            printf("FW info for %s\n", ip6addr_ntoa(&rx_data.src));
                            bm_info_print_from_cbor(&(((uint8_t *)(rx_data.buf->payload))[sizeof(bm_usr_msg_hdr_t)]), payload_length);

                            // Clear address now that we've received data from it
                            ip_addr_set_zero(info_addr);
                        }

                        break;
                    }
                    case MSG_BM_REQUEST_CAPS: {
                        bm_network_send_caps(&rx_data.src);
                        break;
                    }
                    case MSG_BM_CAPS: {
                        printf("Pub/Sub Capabilities for %s\n", ip6addr_ntoa(&rx_data.src));
                        bm_caps_print_from_cbor(&(((uint8_t *)(rx_data.buf->payload))[sizeof(bm_usr_msg_hdr_t)]), payload_length);
                        break;
                    }
                    default: {
                        printf("Unexpected Message received\n");
                        break;
                    }
                }
                /* free the pbuf */
                pbuf_free(rx_data.buf);
            }
        }
    }
}

void bcl_init(SerialHandle_t* hSerial) {
    err_t       mld6_err;
    int         rval;
    ip6_addr_t  multicast_glob_addr;

    printf( "Starting up BCL\n" );

    /* Looks like we don't call lwip_init if we are using a RTOS */
    tcpip_init(NULL, NULL);

    getMacAddr(netif.hwaddr, sizeof(netif.hwaddr));
    netif.hwaddr_len = sizeof(netif.hwaddr);

    // Generate IPv6 address using EUI-64 format derived from mac address
    ip_addr_t my_addr = IPADDR6_INIT_HOST(
            0x20010db8,
            0,
            (netif.hwaddr[0] << 24 | netif.hwaddr[1] << 16 | netif.hwaddr[2] << 8 | 0xFF),
            (0xFE << 24 | netif.hwaddr[3] << 16 | netif.hwaddr[4] << 8 | netif.hwaddr[5])
        );

    inet6_aton("ff03::1", &multicast_glob_addr);

    /* The documentation says to use tcpip_input if we are running with an OS */
    netif_add(&netif, NULL, bm_l2_init, tcpip_input);

    netif_create_ip6_linklocal_address(&netif, 1);
    netif_set_up(&netif);
    netif_ip6_addr_set(&netif, 0, ip_2_ip6(&my_addr));
    netif_ip6_addr_set_state(&netif, 0, IP6_ADDR_VALID);
    netif_set_link_up(&netif);

    /* add to relevant multicast groups */
    mld6_err = mld6_joingroup_netif(&netif, &multicast_glob_addr);
    if (mld6_err != ERR_OK){
        printf("Could not join ff03::1\n");
    }

    /* Create threads and Queues */
    bcl_rx_queue = xQueueCreate( BCL_RX_QUEUE_NUM_ENTRIES, sizeof(bcl_rx_element_t));

    rval = xTaskCreate(bcl_rx_thread,
                       "BCL RX Thread",
                       2048,
                       NULL,
                       BM_NETWORK_TASK_PRIORITY,
                       &rx_thread);
    configASSERT(rval == pdPASS);

    /* Using raw udp tx/rx for now */
    udp_pcb = udp_new_ip_type(IPADDR_TYPE_V6);
    udp_port = BM_BCL_PORT;
    udp_bind(udp_pcb, IP_ANY_TYPE, udp_port);
    udp_recv(udp_pcb, bcl_rx_cb, NULL);

    /* Init and start the bristlemouth network */
    bm_network_init(my_addr, udp_pcb, udp_port, &netif);

    /* Start DFU service here */
    bm_dfu_init(hSerial, my_addr, &netif);


    bm_middleware_init(&netif, BM_MIDDLEWARE_PORT);

    /* FIXME: Why is this delay needed between initializing and sending out neighbor discovery? Without it, any
              messages attempted to be sent withing X ms are not received */
    vTaskDelay(400);
    bm_network_start();

}

/*!
  Get string representation of IP address.
  NOTE: returns ptr to static buffer; not reentrant! (from ip6addr_ntoa)

  \param[in] ip IP address index (Since there are up to LWIP_IPV6_NUM_ADDRESSES)
  \return pointer to string with ip address
*/
const char *bcl_get_ip_str(uint8_t ip) {
    configASSERT(ip < LWIP_IPV6_NUM_ADDRESSES);
    return(ip6addr_ntoa(netif_ip6_addr(&netif, ip)));
}
