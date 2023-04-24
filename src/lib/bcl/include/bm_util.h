#pragma once

#include <stdint.h>
#include "FreeRTOS.h"
#include "lwip/ip_addr.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const ip6_addr_t multicast_global_addr;
extern const ip6_addr_t multicast_ll_addr;

/*!
  Extract 64-bit node id from IP address

  \param *ip address to extract node id from
  \return node id
*/
static inline uint64_t ip_to_nodeid(const ip_addr_t *ip) {
	configASSERT(ip);
    return (uint64_t)__builtin_bswap32(ip->addr[2]) << 32 | __builtin_bswap32(ip->addr[3]);
}

#ifdef __cplusplus
}
#endif
