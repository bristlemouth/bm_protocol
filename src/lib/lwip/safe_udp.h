#pragma once

#include "lwip/tcpip.h"
#include "lwip/udp.h"

#if !LWIP_TCPIP_CORE_LOCKING
#error LWIP_TCPIP_CORE_LOCKING MUST be enabled to use udp_sendto safely!
#endif

static inline err_t safe_udp_sendto(struct udp_pcb *pcb, struct pbuf *p,
           const ip_addr_t *dst_ip, u16_t dst_port) {
  LOCK_TCPIP_CORE();
  err_t rval = udp_sendto(pcb, p, dst_ip, dst_port);
  UNLOCK_TCPIP_CORE();

  return rval;
}

static inline err_t safe_udp_sendto_if(struct udp_pcb *pcb, struct pbuf *p,
           const ip_addr_t *dst_ip, u16_t dst_port, struct netif *netif) {
  LOCK_TCPIP_CORE();
  err_t rval = udp_sendto_if(pcb, p, dst_ip, dst_port, netif);
  UNLOCK_TCPIP_CORE();

  return rval;
}

static inline err_t safe_udp_sendto_if_src(struct udp_pcb *pcb, struct pbuf *p,
           const ip_addr_t *dst_ip, u16_t dst_port, struct netif *netif, const ip_addr_t *src_ip) {
  LOCK_TCPIP_CORE();
  err_t rval = udp_sendto_if_src(pcb, p, dst_ip, dst_port, netif, src_ip);
  UNLOCK_TCPIP_CORE();

  return rval;
}
