#pragma once

#define LWIP_IPV4                         0
#define LWIP_IPV6                         1

#define NO_SYS                            0
#define LWIP_SOCKET                       0
#define LWIP_NETCONN                      0
#define LWIP_NETIF_API                    0

#define LWIP_HAVE_LOOPIF                  0
#define LWIP_NETIF_LOOPBACK               0
#define LWIP_LOOPBACK_MAX_PBUFS           0

#define LWIP_COMPAT_SOCKETS               0

// We MUST use core locking since we're using the raw API, which is not
// thread safe on it's own
// See https://www.nongnu.org/lwip/2_1_x/pitfalls.html for more info
#define LWIP_TCPIP_CORE_LOCKING           1
#define LWIP_FREERTOS_CHECK_CORE_LOCKING  1

#define LWIP_NETIF_LINK_CALLBACK          1
#define LWIP_NETIF_STATUS_CALLBACK        1
#define LWIP_NETIF_EXT_STATUS_CALLBACK    1

#define TCPIP_MBOX_SIZE                   16
#define TCPIP_THREAD_STACKSIZE            8192

// Needed for multicast
#define LWIP_IPV6_MLD                     (LWIP_IPV6)
#define LWIP_MULTICAST_TX_OPTIONS         1

/* Disabling because we are modifying the SRC IPV6 address to hold the ingress port before
   passing the payload to lwip. This will cause the packet to fail the CRC */
#define CHECKSUM_GEN_UDP                  1
#define CHECKSUM_CHECK_UDP                0

// #define LWIP_DEBUG 1

#ifdef LWIP_DEBUG

#define LWIP_DBG_MIN_LEVEL         0
#define PPP_DEBUG                  LWIP_DBG_OFF
#define MEM_DEBUG                  LWIP_DBG_OFF
#define MEMP_DEBUG                 LWIP_DBG_OFF
#define PBUF_DEBUG                 LWIP_DBG_OFF
#define API_LIB_DEBUG              LWIP_DBG_ON
#define API_MSG_DEBUG              LWIP_DBG_ON
#define TCPIP_DEBUG                LWIP_DBG_ON
#define NETIF_DEBUG                LWIP_DBG_ON
#define SOCKETS_DEBUG              LWIP_DBG_OFF
#define DNS_DEBUG                  LWIP_DBG_OFF
#define AUTOIP_DEBUG               LWIP_DBG_OFF
#define DHCP_DEBUG                 LWIP_DBG_OFF
#define IP_DEBUG                   LWIP_DBG_ON
#define IP6_DEBUG                  LWIP_DBG_ON
#define IP_REASS_DEBUG             LWIP_DBG_ON
#define ICMP_DEBUG                 LWIP_DBG_ON
#define IGMP_DEBUG                 LWIP_DBG_ON
#define UDP_DEBUG                  LWIP_DBG_ON
#define TCP_DEBUG                  LWIP_DBG_OFF
#define TCP_INPUT_DEBUG            LWIP_DBG_OFF
#define TCP_OUTPUT_DEBUG           LWIP_DBG_OFF
#define TCP_RTO_DEBUG              LWIP_DBG_OFF
#define TCP_CWND_DEBUG             LWIP_DBG_OFF
#define TCP_WND_DEBUG              LWIP_DBG_OFF
#define TCP_FR_DEBUG               LWIP_DBG_OFF
#define TCP_QLEN_DEBUG             LWIP_DBG_OFF
#define TCP_RST_DEBUG              LWIP_DBG_OFF
#endif

#define LWIP_DBG_TYPES_ON         (LWIP_DBG_ON|LWIP_DBG_TRACE|LWIP_DBG_STATE|LWIP_DBG_FRESH|LWIP_DBG_HALT)


/* ---------- Memory options ---------- */
/* MEM_ALIGNMENT: should be set to the alignment of the CPU for which
   lwIP is compiled. 4 byte alignment -> define MEM_ALIGNMENT to 4, 2
   byte alignment -> define MEM_ALIGNMENT to 2. */
/* MSVC port: intel processors don't need 4-byte alignment,
   but are faster that way! */
#define MEM_ALIGNMENT           4U

/* MEM_SIZE: the size of the heap memory. If the application will send
a lot of data that needs to be copied, this should be set high. */
#define MEM_SIZE               128 * 1024

/* MEMP_NUM_PBUF: the number of memp struct pbufs. If the application
   sends a lot of data out of ROM (or other static memory), this
   should be set high. */
#define MEMP_NUM_PBUF           0

/* MEMP_NUM_RAW_PCB: the number of UDP protocol control blocks. One
   per active RAW "connection". */
#define MEMP_NUM_RAW_PCB        3
/* MEMP_NUM_UDP_PCB: the number of UDP protocol control blocks. One
   per active UDP "connection". */
#define MEMP_NUM_UDP_PCB        8
/* MEMP_NUM_TCP_PCB: the number of simulatenously active TCP
   connections. */
#define MEMP_NUM_TCP_PCB        0
/* MEMP_NUM_TCP_PCB_LISTEN: the number of listening TCP
   connections. */
#define MEMP_NUM_TCP_PCB_LISTEN 0
/* MEMP_NUM_TCP_SEG: the number of simultaneously queued TCP
   segments. */
#define MEMP_NUM_TCP_SEG        0
/* MEMP_NUM_SYS_TIMEOUT: the number of simulateously active
   timeouts. */
// #define MEMP_NUM_SYS_TIMEOUT    17

/* The following four are used only with the sequential API and can be
   set to 0 if the application only will use the raw API. */
/* MEMP_NUM_NETBUF: the number of struct netbufs. */
#define MEMP_NUM_NETBUF         0
/* MEMP_NUM_NETCONN: the number of struct netconns. */
#define MEMP_NUM_NETCONN        0
/* MEMP_NUM_TCPIP_MSG_*: the number of struct tcpip_msg, which is used
   for sequential API communication and incoming packets. Used in
   src/api/tcpip.c. */
#define MEMP_NUM_TCPIP_MSG_API   0
#define MEMP_NUM_TCPIP_MSG_INPKT 0


/* ---------- Pbuf options ---------- */
/* PBUF_POOL_SIZE: the number of buffers in the pbuf pool. */
// We're not using the PBUF_POOL right now
#define PBUF_POOL_SIZE          0

/* PBUF_POOL_BUFSIZE: the size of each pbuf in the pbuf pool. */
#define PBUF_POOL_BUFSIZE       256

/** SYS_LIGHTWEIGHT_PROT
 * define SYS_LIGHTWEIGHT_PROT in lwipopts.h if you want inter-task protection
 * for certain critical regions during buffer allocation, deallocation and memory
 * allocation and deallocation.
 */
#define SYS_LIGHTWEIGHT_PROT    (NO_SYS==0)


/* ---------- TCP options ---------- */
#define LWIP_TCP                0

// We're not using ARP right now
#define LWIP_ARP                0

// Have to enable ethernet directly since ARP is disabled
#define LWIP_ETHERNET 1


// Don't forward or do ipv6 fragmenting
#define LWIP_IPV6_FORWARD 0
#define LWIP_IPV6_FRAG 0
#define LWIP_IPV6_REASS 0


#define LWIP_DHCP               0


/* ---------- UDP options ---------- */
#define LWIP_UDP                1

#define UDP_TTL                 255


/* ---------- RAW options ---------- */
#define LWIP_RAW                1

// Multicast options


/* ---------- Statistics options ---------- */

#define LWIP_STATS              0
#define LWIP_STATS_DISPLAY      0

#if LWIP_STATS
#define LINK_STATS              1
#define IP_STATS                1
#define ICMP_STATS              0
#define IGMP_STATS              0
#define IPFRAG_STATS            0
#define UDP_STATS               1
#define TCP_STATS               0
#define MEM_STATS               1
#define MEMP_STATS              1
#define PBUF_STATS              1
#define SYS_STATS               1
#endif /* LWIP_STATS */


/* The following defines must be done even in OPTTEST mode: */

#if !defined(NO_SYS) || !NO_SYS /* default is 0 */
void sys_check_core_locking(void);
#define LWIP_ASSERT_CORE_LOCKED()  sys_check_core_locking()
#endif

#ifndef LWIP_PLATFORM_ASSERT
/* Define LWIP_PLATFORM_ASSERT to something to catch missing stdio.h includes */
void lwip_example_app_platform_assert(const char *msg, int line, const char *file);
#define LWIP_PLATFORM_ASSERT(x) lwip_example_app_platform_assert(x, __LINE__, __FILE__)
#endif

#if LWIP_DEBUG
#include <stdio.h>
#define LWIP_PLATFORM_DIAG(x) do {printf x;} while(0)
#endif
