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
#include "lwip/sockets.h"

#include "bm_l2.h"

static struct netif     netif;
static struct udp_pcb   *udp_pcb;
static uint16_t         udp_port;
static ip6_addr_t       multicast_glob_addr;
static ip6_addr_t       multicast_ll_addr;
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
                printf("Received: %s\n", (char*) rx_data.buf->payload);
                printf("Src IP Addr = %s\n", ip6addr_ntoa(&(rx_data.src)));
                /* free the pbuf */
                pbuf_free(rx_data.buf);
            }
        }
    }
}


void bcl_init(void)
{
    struct pbuf* buf = NULL;
    uint8_t msg[] = "Hello World";
    err_t mld6_err;
    int rval;

    printf( "Starting up BCL\n" );

    /* Looks like we don't call lwip_init if we are using a RTOS */
    tcpip_init(NULL, NULL);

    getMacAddr(netif.hwaddr, sizeof(netif.hwaddr));
    netif.hwaddr_len = sizeof(netif.hwaddr);

    /* FIXME: Let's not hardcode this if possible */
    ip_addr_t my_addr = IPADDR6_INIT_HOST(0x20010db8, 0x0, 0x0, IPV6_ADDR_LSB);

    inet6_aton("ff02::1", &multicast_ll_addr);
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

    while(1) {
        buf = pbuf_alloc(PBUF_TRANSPORT, sizeof(msg), PBUF_RAM);
        memcpy (buf->payload, msg, sizeof(msg));
        udp_sendto_if(udp_pcb, buf, &multicast_glob_addr, udp_port, &netif);
        vTaskDelay(100);
        pbuf_free(buf);
    }

}

