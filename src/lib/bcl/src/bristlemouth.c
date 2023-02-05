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

#include <zcbor_common.h>
#include <zcbor_decode.h>
#include <zcbor_encode.h>

#include "bm_zcbor_decode.h"
#include "bm_zcbor_encode.h"
#include "bm_usr_msg.h"
#include "bm_msg_types.h"
#include "bm_network.h"

#include "bm_l2.h"

static struct netif     netif;
static struct udp_pcb*  udp_pcb;
static uint16_t         udp_port;
static TaskHandle_t     rx_thread = NULL;
static QueueHandle_t    bcl_rx_queue;

// Get IPv6 LSB from CMAKE ALT_DEV define (use 0x01, 0x02, etc...)
#define IPV6_ADDR_LSB ALT_DEV

/* Define here for now */
#define BCL_RX_QUEUE_NUM_ENTRIES  5

typedef struct bcl_rx_element_s {
    struct pbuf* buf;
    ip_addr_t src;
    //other_info_t other_info;
} bcl_rx_element_t;

static void bcl_rx_cb(void *arg, struct udp_pcb *upcb, struct pbuf *p,
                 const ip_addr_t *addr, u16_t port) {
    (void) arg;
    (void) upcb;
    (void) addr;
    (void) port;
    static bcl_rx_element_t rx_data;

    if (p != NULL) {
        rx_data.buf = p;
        rx_data.src = *addr;
        if(xQueueSend(bcl_rx_queue, &rx_data, 0) != pdTRUE) {
            printf("Error sending to Queue\n");
        }
    }
}

static void bcl_rx_thread(void *parameters) {
    (void) parameters;

    static bcl_rx_element_t rx_data;
    while (1) {
        if(xQueueReceive(bcl_rx_queue, &rx_data, portMAX_DELAY) == pdPASS) {
            if (rx_data.buf != NULL) {
                /* We assume that a bm_usr_msg_hdr_t precedes the payload.
                   Inspect the message type ID */
                switch (((bm_usr_msg_hdr_t* ) rx_data.buf->payload)->id) {
                    case MSG_BM_ACK:
                        printf("Received ACK\n");
                        break;
                    case MSG_BM_HEARTBEAT:
                        printf("Received Heartbeat\n");
                        break;
                    case MSG_BM_DISCOVER_NEIGHBORS:
                        printf("Received Neighbor Discovery\n");
                        break;
                    case MSG_BM_DURATION:
                        printf("Received Duration\n");
                        break;
                    case MSG_BM_TIME:
                        printf("Received Time\n");
                        break;
                    case MSG_BM_REQUEST_TABLE:
                        printf("Received Table Request\n");
                        break;
                    case MSG_BM_TABLE_RESPONSE:
                        printf("Received Table Response\n");
                        break;
                    default:
                        break;
                }

                printf("Src IP Addr = %s\n", ip6addr_ntoa(&(rx_data.src)));

                /* free the pbuf */
                pbuf_free(rx_data.buf);
            }
        }
    }
}

void bcl_init(void) {
    err_t       mld6_err;
    int         rval;
    ip6_addr_t  multicast_glob_addr;

    printf( "Starting up BCL\n" );

    /* Looks like we don't call lwip_init if we are using a RTOS */
    tcpip_init(NULL, NULL);

    getMacAddr(netif.hwaddr, sizeof(netif.hwaddr));
    netif.hwaddr_len = sizeof(netif.hwaddr);

    /* FIXME: Let's not hardcode this if possible */
    ip_addr_t my_addr = IPADDR6_INIT_HOST(0x20010db8, 0, 0, IPV6_ADDR_LSB);

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
                       15,
                       &rx_thread);
    configASSERT(rval == pdPASS);

    /* Using raw udp tx/rx for now */
    udp_pcb = udp_new_ip_type(IPADDR_TYPE_V6);
    udp_port = 2222;
    udp_bind(udp_pcb, IP_ANY_TYPE, udp_port);
    udp_recv(udp_pcb, bcl_rx_cb, NULL);

    /* Init and start the bristlemouth network */
    bm_network_init(my_addr, udp_pcb, udp_port, &netif);

    /* FIXME: Why is this delay needed between initializing and sending out neighbor discovery? Without it, any 
              messages attempted to be sent withing X ms are not received */
    vTaskDelay(400);

    bm_network_start();
}

